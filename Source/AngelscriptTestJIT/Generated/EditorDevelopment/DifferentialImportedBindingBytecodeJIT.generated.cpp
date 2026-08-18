// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : BytecodeJIT
 * Declaration        : int SemanticImportedBinding(const int)
 * StableFunctionKey  : cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746
 * OriginalSymbolSet  : ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_d058635e3c75e6d21eaea3526da99683d0b06744dd8a3db4eb3599d34dfd4585
 * TestSymbolSet      : AS_Test_Differential_Bytecode_SemanticImportedBinding
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
 * Declaration       : int SemanticImportedBinding(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:2
 * StableFunctionKey  : cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746
 * ExecutionHash      : d058635e3c75e6d21eaea3526da99683d0b06744dd8a3db4eb3599d34dfd4585
 * DebugHash          : 4e812d41acefcdde1a5d04df3f8a0ddbbcb2dee67d6a67d001583b4ee7660916
 * EntryAbiHash       : a4b711dd5134aeaca25a93796be749414190495c109eeba3fb1015c75b1a90b1
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_AS_Test_Differential_Bytecode_SemanticImportedBinding = "ASStaticJITTypedFinalScalarFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_AS_Test_Differential_Bytecode_SemanticImportedBinding
#endif

// AS Function : int SemanticImportedBinding(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:2
// JIT Entry   : Raw
int32 AS_Test_Differential_Bytecode_SemanticImportedBinding(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UTypedASTFinalScalarReceiver::SemanticImportedBinding(int)", 22);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[4];
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
// PshV4 v-2
// CALLBND
// int ImportedValueForAOT(const int)
SCRIPT_DEBUG_CALLSTACK_POSITION(22, 3);
{
  asCScriptFunction* SignatureFunc = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
  if (SignatureFunc == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  int FunctionId = SCRIPT_ENGINE->importedFunctions[SignatureFunc->GetId() & ~FUNC_IMPORTED]->boundFunctionId;
  if (FunctionId == -1) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  asCScriptFunction* CallFunction = SCRIPT_ENGINE->scriptFunctions[FunctionId];
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetArgDWord(0, value_as<asDWORD>((p_Value)));
  {
     FScopeStaticJITNestedExceptionAdoption ExceptionAdoption(*CallContext);
     CallContext->Execute();
  }
  if (Execution.bExceptionThrown || CallContext->m_status != asEXECUTION_FINISHED)
  {
     FStaticJITFunction::AdoptContextException(Execution, *CallContext);
     Execution.bExceptionThrown = true;
return {};
  }
  l_valueRegister = CallContext->m_regs.valueRegister;
}
// JitEntry *
// CpyRtoV4 v1
memcpy((&v_TEMP_dword_1), &l_valueRegister, 4);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 3
  return (int32)l_dwordRegister;
}
// AS Function : int SemanticImportedBinding(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:2
// JIT Entry   : VM
void AS_Test_Differential_Bytecode_SemanticImportedBinding_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_Bytecode_SemanticImportedBinding(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int SemanticImportedBinding(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:2
// JIT Entry   : Parms
void AS_Test_Differential_Bytecode_SemanticImportedBinding_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_Differential_Bytecode_SemanticImportedBinding(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FBytecodeJITDifferentialEntryRegistrationReferenceSlots[2] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(2), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("fad89b1b314e72fddfda397e555d92b3e881dce5158330451090697253e7b4e8")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("85b780d3d0320f0320f21f66806418c53611e2e04d160beb79f3c8a598f7a9bd")))}}},
		{sizeof(FAngelscriptJITReferenceSlot), 1u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(6), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("3be17b2315862baa649d754fe3506a131bfd1ce3ec2e4659f5c5808d8bbf1e64")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("572753028c67cb22d347f9b0040f6b2bf66d8973a68bfd51259e63c12687bd02")))}}},
	};

	struct FBytecodeJITDifferentialEntryRegistration
	{
		FBytecodeJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::BytecodeJIT,
				&AS_Test_Differential_Bytecode_SemanticImportedBinding_VMEntry,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_Bytecode_SemanticImportedBinding),
				&AS_Test_Differential_Bytecode_SemanticImportedBinding_ParmsEntry,
				TEXT("AS_Test_Differential_Bytecode_SemanticImportedBinding"),
				TEXT("cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746"),
				TEXT("int SemanticImportedBinding(const int)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as"),
				20u,
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
