// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : BytecodeJIT
 * Declaration        : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
 * StableFunctionKey  : 9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4
 * OriginalSymbolSet  : ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_3d287fec96570ae323b5be5f87799c96aff6472b47dfb88cd4de99ab40b03762
 * TestSymbolSet      : AS_Test_Differential_Bytecode_TypedASTPrintShowcase
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
 * Declaration       : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
 * Source             : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:1
 * StableFunctionKey  : 9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4
 * ExecutionHash      : 3d287fec96570ae323b5be5f87799c96aff6472b47dfb88cd4de99ab40b03762
 * DebugHash          : 636443155f3600493c3c1febfd095150c987de2da9db64ac7a9fb82658c7520b
 * EntryAbiHash       : d2d096b4d79e2578e2eb39dab9d2de5cff1e98d1f6686e58ef0a05e5459586c4
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_AS_Test_Differential_Bytecode_TypedASTPrintShowcase = "ASStaticJITTypedPrintFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_AS_Test_Differential_Bytecode_TypedASTPrintShowcase
#endif

// AS Function : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:1
// JIT Entry   : Raw
int32 AS_Test_Differential_Bytecode_TypedASTPrintShowcase(FScriptExecution& Execution, FString* p_Text, double p_Duration, FLinearColor* p_Color)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int TypedASTPrintShowcase(FString, float, FLinearColor)", 7);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[24];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
FLinearColor* v_TEMP_ptr_2 = nullptr;
float v_TEMP_float_3 = {};
asDWORD v_TEMP_dword_4 = {};
void* AllocMemory;
// TrackRef v0
// TrackRef v-4
// JitEntry *
// SUSPEND
// JitEntry *
// PSF v2
// ValidateRef v-4
// PshVPtr v-4
// ALLOC
{
  asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 1u);
  AllocMemory = (void*)SCRIPT_ENGINE->CallAlloc(objType);
}
value_assign_safe<asQWORD>(&l_stack[0], AllocMemory);
// FLinearColor::FLinearColor(const FLinearColor&inout Other)
{
  FLinearColor* Object = (FLinearColor*)((asQWORD&)l_stack[0]);
SCRIPT_DEBUG_CALLSTACK_POSITION(7, 2);
  if(Object != nullptr)
  {
new (Object) FLinearColor((*(FLinearColor*)(p_Color)));
} else [[unlikely]] {
SCRIPT_NULL_POINTER_EXCEPTION();
return {};
}
}
{
  asPWORD* a = (asPWORD*)((&v_TEMP_ptr_2));
  if(a != nullptr) *a = (asPWORD)AllocMemory;
}
// JitEntry *
// VAR v2
// dTOf v3, v-2
v_TEMP_float_3 = float(p_Duration);
// PshV4 v3
// ValidateRef v0
// PshVPtr v0
// GETOBJ
  asPWORD v = (asPWORD)(&v_TEMP_ptr_2);
// Materialize stack 4
value_assign_safe<asQWORD>(&l_stack[16], *(asPWORD*)v);
  *(asPWORD*)v = 0;
// CALLSYS *
// void Print(const FString&inout Text, float32 Duration = 5.f, FLinearColor Color = FLinearColor :: LucBlue)
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 2u).GetFunction();
  auto CastedFuncPtr = (void(*)(void*,float,FLinearColor))RawFuncPtr;
SCRIPT_DEBUG_CALLSTACK_POSITION(7, 2);
CastedFuncPtr((void**)(p_Text),value_as<float>((v_TEMP_float_3)),*(FLinearColor*)((asQWORD&)l_stack[16]));
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// SetV4 v4, 314
v_TEMP_dword_4 = 0x13au;
// CpyVtoR4 v4
l_dwordRegister = v_TEMP_dword_4;
// UntrackRef v0
// UntrackRef v-4
// RET 6
  return (int32)l_dwordRegister;
}
// AS Function : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:1
// JIT Entry   : VM
void AS_Test_Differential_Bytecode_TypedASTPrintShowcase_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_Bytecode_TypedASTPrintShowcase(Execution,
		*(FString**)(l_fp + 0),
		*(double*)(l_fp + 2),
		*(FLinearColor**)(l_fp + 4));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FBytecodeJITDifferentialEntryRegistrationReferenceSlots[4] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("415ec867beab32a47d558983ab77291ac493547168161b93836dc77a357f059a")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("4966ab4336cca1891b1b828360f0d1a587dad0189ac8dc042b1dde592cb032d5")))}}},
		{sizeof(FAngelscriptJITReferenceSlot), 1u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("cc621ee62270e50578779a415d2789dc2b5e1a70d1ea5a785cbd60e43233c192")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("dfcf882626eca6dff9e5a227f732555955dceaef4baf13aa55a7b1e7839c39ef")))}}},
		{sizeof(FAngelscriptJITReferenceSlot), 2u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("cffdb228484b4358e4f3d607d572055b99963e418b0765f6dd934433adcbfca3")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("86168bbdfe709e4f79413ae5fcdfb97fb18587ec710ec2cd4727e5bb6913597d")))}}},
		{sizeof(FAngelscriptJITReferenceSlot), 3u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("fce382e1f207809532a575043a4e727bc6fb503d99b6b8700fd77d71906a1080")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("207b6b88fbdf7c084cb14ddada1e386a939d1754325d527f699e94d41cd5de56")))}}},
	};

	struct FBytecodeJITDifferentialEntryRegistration
	{
		FBytecodeJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::BytecodeJIT,
				&AS_Test_Differential_Bytecode_TypedASTPrintShowcase_VMEntry,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_Bytecode_TypedASTPrintShowcase),
				nullptr,
				TEXT("AS_Test_Differential_Bytecode_TypedASTPrintShowcase"),
				TEXT("9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4"),
				TEXT("int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedPrintFixture.as"),
				2u,
				1u,
				AngelscriptTestJITProbes::EIsolatedDifferentialCleanupPlanState::NotCaptured,
				0u,
				FBytecodeJITDifferentialEntryRegistrationReferenceSlots,
				4u);
		}
	};

	FBytecodeJITDifferentialEntryRegistration GBytecodeJITDifferentialEntryRegistration;
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
