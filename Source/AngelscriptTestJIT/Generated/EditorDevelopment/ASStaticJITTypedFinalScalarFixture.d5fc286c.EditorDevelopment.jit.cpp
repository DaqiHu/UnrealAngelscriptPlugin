// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as
 * CanonicalModuleName : ASStaticJITTypedFinalScalarFixture
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : d5fc286cb1b96446cf9ab866b8485d617ca75fc6ed7bfa9cf90f88338e6e615a
 * ProviderId          : 7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44
 * ArtifactProfile     : 018cc71ab6612eca7608aa13abe1f7a1a7d81964bb8cc5821f1b1e047fcf7a11
 * NativeEnvironment   : 76d620ec7d0db38aa2028996e595f75523f201e7e560c05d8c9000d60d04cd73
 * FunctionCount       : 7
 */

#if WITH_EDITOR && UE_BUILD_DEVELOPMENT
#include "StaticJIT/StaticJITConfig.h"
#ifndef AS_SKIP_JITTED_CODE
#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallables.h"
#include "StaticJIT/StaticJITHeader.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"
constexpr SIZE_T POFFSET_UStaticJITAotFunctionCarrier_StoredValue = Align(sizeof(UObject) + 0, 4);
constexpr SIZE_T PALIGN_UStaticJITAotFunctionCarrier_StoredValue = AlignmentMax(alignof(UObject), 4);
constexpr SIZE_T TALIGN_UStaticJITAotFunctionCarrier = PALIGN_UStaticJITAotFunctionCarrier_StoredValue;
constexpr SIZE_T TSIZE_UStaticJITAotFunctionCarrier = Align(POFFSET_UStaticJITAotFunctionCarrier_StoredValue + 4, TALIGN_UStaticJITAotFunctionCarrier);
/*
 * AngelScript Static JIT Function
 * Declaration       : float SemanticPowerBoundary(const float, const float)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:2
 * StableFunctionKey  : 23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67
 * ExecutionHash      : 9dbf977650aa4edb4b93031a8dd0deaeb606af68a3eed3cab50d0fbdf8b0aa62
 * DebugHash          : 740f5496c25fc7d3489cdb4b02baf5f276ff7221015973e38360b2812a97c9fe
 * EntryAbiHash       : 5bdf2107db2403a739f0c1ec988a215ef8d4496714c3a85248de4057037ca025
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static double ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_9dbf977650aa4edb4b93031a8dd0deaeb606af68a3eed3cab50d0fbdf8b0aa62_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const double as_ast_d20,
	const double as_ast_d21);

static double ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_9dbf977650aa4edb4b93031a8dd0deaeb606af68a3eed3cab50d0fbdf8b0aa62_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const double as_ast_d20,
	const double as_ast_d21)
{
	{
		TypedFrame.SetSourcePosition(77, 3);
		return ([&]() -> double
	{
		TypedFrame.SetSourcePosition(77, 3);
		const double as_ast_e81_left = as_ast_d20;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		const double as_ast_e81_right = as_ast_d21;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		return AngelscriptTypedASTJIT::CheckedPower<double>(Execution, as_ast_e81_left, as_ast_e81_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return double{};
}

// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:2
// JIT Entry   : Raw
double ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_9dbf977650aa4edb4b93031a8dd0deaeb606af68a3eed3cab50d0fbdf8b0aa62_Raw(FScriptExecution& Execution, void* Object, double p_0, double p_1)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as",
		"float SemanticPowerBoundary(const float, const float)",
		73,
		Object,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_9dbf977650aa4edb4b93031a8dd0deaeb606af68a3eed3cab50d0fbdf8b0aa62_TypedBody(Execution, TypedFrame, static_cast<double>(p_0), static_cast<double>(p_1));
}

// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:2
// JIT Entry   : VM
void ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_9dbf977650aa4edb4b93031a8dd0deaeb606af68a3eed3cab50d0fbdf8b0aa62_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(double*)l_outValue = ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_9dbf977650aa4edb4b93031a8dd0deaeb606af68a3eed3cab50d0fbdf8b0aa62_Raw(Execution,
		*(void**)l_fp,
		*(double*)(l_fp + 2),
		*(double*)(l_fp + 4));
}

// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:2
// JIT Entry   : Parms
void ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_9dbf977650aa4edb4b93031a8dd0deaeb606af68a3eed3cab50d0fbdf8b0aa62_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(double*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_9dbf977650aa4edb4b93031a8dd0deaeb606af68a3eed3cab50d0fbdf8b0aa62_Raw(Execution,
		Object,
		static_cast<double>(*(double*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<double>(*(double*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UTypedASTFinalScalarReceiver UTypedASTFinalScalarReceiver()
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
 * StableFunctionKey  : b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f
 * ExecutionHash      : 4b77f32c6394226f827e14165ce0e815b31ddf6fe40b93c49224bc6975a68a4f
 * DebugHash          : dd69e7ecceb9074bb1f98bb16a5778455a2cfc64821e0d12c0e8a737fd56514c
 * EntryAbiHash       : 5d9f493ad48c6524ca33a73c311249bdf7a4315a86649957cedb5fedfe5acda5
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_4b77f32c6394226f827e14165ce0e815b31ddf6fe40b93c49224bc6975a68a4f = "ASStaticJITTypedFinalScalarFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_4b77f32c6394226f827e14165ce0e815b31ddf6fe40b93c49224bc6975a68a4f
#endif

// AS Function : UTypedASTFinalScalarReceiver UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
// JIT Entry   : Raw
UObject* ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_4b77f32c6394226f827e14165ce0e815b31ddf6fe40b93c49224bc6975a68a4f(FScriptExecution& Execution)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("UTypedASTFinalScalarReceiver UTypedASTFinalScalarReceiver()", 0);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[16];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
void* v_TEMP_2 = nullptr;
// JitEntry *
// PSF v2
// ALLOC
{
  asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 0u);
  asDWORD* mem = (asDWORD*)SCRIPT_ENGINE->AllocScriptObject(objType);
  ScriptObject_Construct(objType, (asCScriptObject*)mem);
  asPWORD* a = (asPWORD*)((&v_TEMP_2));
  if(a != nullptr) *a = (asPWORD)mem;
value_assign_safe<asQWORD>(&l_stack[0], mem);
// UTypedASTFinalScalarReceiver::UTypedASTFinalScalarReceiver()
SCRIPT_DEBUG_CALLSTACK_POSITION(0, 0);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609")))}}}}, CallFunction, (void*)(((asQWORD&)l_stack[0])), true);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetObject((void*)((asQWORD&)l_stack[0]));
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
}
}
// JitEntry *
// LOADOBJ v2
l_objectRegister = (void*)v_TEMP_2;
v_TEMP_2 = nullptr;
// RET 0
  return (UObject*)l_objectRegister;
}
// AS Function : UTypedASTFinalScalarReceiver UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
// JIT Entry   : VM
void ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_4b77f32c6394226f827e14165ce0e815b31ddf6fe40b93c49224bc6975a68a4f_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_4b77f32c6394226f827e14165ce0e815b31ddf6fe40b93c49224bc6975a68a4f(Execution);
}
// AS Function : UTypedASTFinalScalarReceiver UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
// JIT Entry   : Parms
void ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_4b77f32c6394226f827e14165ce0e815b31ddf6fe40b93c49224bc6975a68a4f_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_4b77f32c6394226f827e14165ce0e815b31ddf6fe40b93c49224bc6975a68a4f(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int DifferentialScalarValue(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:2
 * StableFunctionKey  : bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b
 * ExecutionHash      : e8048493f15f25a9795b805ece588735db7476836c7b75f94df6059f532def96
 * DebugHash          : ecf76d4c9b112b12596ea777d3e92bd73c591af07200b418e57b4271fa199f5c
 * EntryAbiHash       : dabb76ccb243078a57121d4c3a40ed732a70a68cca7584d9e9320a1fd5a679c4
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_e8048493f15f25a9795b805ece588735db7476836c7b75f94df6059f532def96_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d9,
	const int32 as_ast_d10);

static int32 ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_e8048493f15f25a9795b805ece588735db7476836c7b75f94df6059f532def96_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d9,
	const int32 as_ast_d10)
{
	{
		TypedFrame.SetSourcePosition(16, 3);
		return ([&]() -> int32
	{
		const int32 as_ast_e9_left = ([&]() -> int32
	{
		const int32 as_ast_e8_left = ([&]() -> int32
	{
		const int32 as_ast_e7_left = as_ast_d9;
		const int32 as_ast_e7_right = int32(3);
		return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e7_left, as_ast_e7_right);
	}());
		const int32 as_ast_e8_right = as_ast_d10;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e8_left, as_ast_e8_right);
	}());
		const int32 as_ast_e9_right = int32(7);
		return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e9_left, as_ast_e9_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int DifferentialScalarValue(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:2
// JIT Entry   : Raw
int32 ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_e8048493f15f25a9795b805ece588735db7476836c7b75f94df6059f532def96_Raw(FScriptExecution& Execution, void* Object, asDWORD p_0, asDWORD p_1)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as",
		"int DifferentialScalarValue(const int, const int)",
		14,
		Object,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_e8048493f15f25a9795b805ece588735db7476836c7b75f94df6059f532def96_TypedBody(TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int DifferentialScalarValue(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:2
// JIT Entry   : VM
void ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_e8048493f15f25a9795b805ece588735db7476836c7b75f94df6059f532def96_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_e8048493f15f25a9795b805ece588735db7476836c7b75f94df6059f532def96_Raw(Execution,
		*(void**)l_fp,
		*(asDWORD*)(l_fp + 2),
		*(asDWORD*)(l_fp + 3));
}

// AS Function : int DifferentialScalarValue(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:2
// JIT Entry   : Parms
void ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_e8048493f15f25a9795b805ece588735db7476836c7b75f94df6059f532def96_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_e8048493f15f25a9795b805ece588735db7476836c7b75f94df6059f532def96_Raw(Execution,
		Object,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticImportedBinding(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:2
 * StableFunctionKey  : cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746
 * ExecutionHash      : 2552c0d4bc17e8526c201b1b1883ed0a463ac412621a01fd355c13e8004ddce9
 * DebugHash          : 10c105f320391f5d6c07ec1e9bd72f838f899c34363ecadf69defca9cbd6cb5d
 * EntryAbiHash       : a4b711dd5134aeaca25a93796be749414190495c109eeba3fb1015c75b1a90b1
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_2552c0d4bc17e8526c201b1b1883ed0a463ac412621a01fd355c13e8004ddce9 = "ASStaticJITTypedFinalScalarFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_2552c0d4bc17e8526c201b1b1883ed0a463ac412621a01fd355c13e8004ddce9
#endif

// AS Function : int SemanticImportedBinding(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:2
// JIT Entry   : Raw
int32 ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_2552c0d4bc17e8526c201b1b1883ed0a463ac412621a01fd355c13e8004ddce9(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("int UTypedASTFinalScalarReceiver::SemanticImportedBinding(int)", 21);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[4];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// PshV4 v-2
// CALL
// int ImportedValueForAOT(const int Value)
SCRIPT_DEBUG_CALLSTACK_POSITION(22, 3);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("69a0ada34d03f3f5ff2259420f361c1c3aca5cffc47a6b52965cb976fd8f2304")))}}}}, CallFunction, nullptr, false);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
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
// CpyRtoV4 v2
memcpy((&v_TEMP_dword_2), &l_valueRegister, 4);
// CpyVtoV4 v1, v2
v_TEMP_dword_1 = v_TEMP_dword_2;
// JMP 0
goto LABEL_UTypedASTFinalScalarReceiver__SemanticImportedBinding_22;
LABEL_UTypedASTFinalScalarReceiver__SemanticImportedBinding_22:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 3
  return (int32)l_dwordRegister;
}
// AS Function : int SemanticImportedBinding(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:2
// JIT Entry   : VM
void ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_2552c0d4bc17e8526c201b1b1883ed0a463ac412621a01fd355c13e8004ddce9_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_2552c0d4bc17e8526c201b1b1883ed0a463ac412621a01fd355c13e8004ddce9(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int SemanticImportedBinding(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:2
// JIT Entry   : Parms
void ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_2552c0d4bc17e8526c201b1b1883ed0a463ac412621a01fd355c13e8004ddce9_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_2552c0d4bc17e8526c201b1b1883ed0a463ac412621a01fd355c13e8004ddce9(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UTypedASTFinalScalarReceiver()
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
 * StableFunctionKey  : d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609
 * ExecutionHash      : d8b375e58451e0caf328d52e3e4046c47093b4dbdecf6aa4a9762ba860eee9ba
 * DebugHash          : 8c9386bbfb77ad77317d4d9afc3f704ffd4efe223db8194d87a50c479fe12b5a
 * EntryAbiHash       : e7fe3e49a0111396d17b5901011b806a76bd6f4b076fedbb28cc2e4f38d7d5b5
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_d8b375e58451e0caf328d52e3e4046c47093b4dbdecf6aa4a9762ba860eee9ba = "ASStaticJITTypedFinalScalarFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_d8b375e58451e0caf328d52e3e4046c47093b4dbdecf6aa4a9762ba860eee9ba
#endif

// AS Function : UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
// JIT Entry   : Raw
void ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_d8b375e58451e0caf328d52e3e4046c47093b4dbdecf6aa4a9762ba860eee9ba(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UTypedASTFinalScalarReceiver::UTypedASTFinalScalarReceiver()", 6);
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
// SetV4 v1, 0
v_TEMP_dword_1 = 0x0u;
// SUSPEND
// JitEntry *
// PshVPtr v0
// FinConstruct *
{
  asIScriptObject* Object = (asIScriptObject*)(l_This);
  asITypeInfo* TypeInfo = (asITypeInfo*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 0u);
  SCRIPT_FINISH_CONSTRUCT(Object, TypeInfo);
}
// SetV4 v1, 1
v_TEMP_dword_1 = 0x1u;
// RET 2
  return;
}
// AS Function : UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
// JIT Entry   : VM
void ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_d8b375e58451e0caf328d52e3e4046c47093b4dbdecf6aa4a9762ba860eee9ba_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_d8b375e58451e0caf328d52e3e4046c47093b4dbdecf6aa4a9762ba860eee9ba(Execution,
		*(UObject**)l_fp);
}
// AS Function : UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
// JIT Entry   : Parms
void ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_d8b375e58451e0caf328d52e3e4046c47093b4dbdecf6aa4a9762ba860eee9ba_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_d8b375e58451e0caf328d52e3e4046c47093b4dbdecf6aa4a9762ba860eee9ba(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedFinalRawValue()
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:8:2
 * StableFunctionKey  : d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0
 * ExecutionHash      : 13a2c1338b3fa0d9e097c6c352de4e1055c5e5cf95aa1ba6cd9706783b316ece
 * DebugHash          : efba570067e3c31501522218c8758cea185441ddcccc5e47258eddc05a11db89
 * EntryAbiHash       : 855bd98169313400b8f1d6686e5875bbad13825c3cc12fdde4e9ad4afcdbc31f
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_13a2c1338b3fa0d9e097c6c352de4e1055c5e5cf95aa1ba6cd9706783b316ece_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame);

static int32 ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_13a2c1338b3fa0d9e097c6c352de4e1055c5e5cf95aa1ba6cd9706783b316ece_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame)
{
	{
		TypedFrame.SetSourcePosition(10, 3);
		return int32(84);
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int TypedFinalRawValue()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:8:2
// JIT Entry   : Raw
int32 ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_13a2c1338b3fa0d9e097c6c352de4e1055c5e5cf95aa1ba6cd9706783b316ece_Raw(FScriptExecution& Execution, void* Object)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as",
		"int TypedFinalRawValue()",
		8,
		Object,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_13a2c1338b3fa0d9e097c6c352de4e1055c5e5cf95aa1ba6cd9706783b316ece_TypedBody(TypedFrame);
}

// AS Function : int TypedFinalRawValue()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:8:2
// JIT Entry   : VM
void ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_13a2c1338b3fa0d9e097c6c352de4e1055c5e5cf95aa1ba6cd9706783b316ece_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_13a2c1338b3fa0d9e097c6c352de4e1055c5e5cf95aa1ba6cd9706783b316ece_Raw(Execution,
		*(void**)l_fp);
}

// AS Function : int TypedFinalRawValue()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:8:2
// JIT Entry   : Parms
void ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_13a2c1338b3fa0d9e097c6c352de4e1055c5e5cf95aa1ba6cd9706783b316ece_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_13a2c1338b3fa0d9e097c6c352de4e1055c5e5cf95aa1ba6cd9706783b316ece_Raw(Execution,
		Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticNumericBoundary(const int, const int, const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
 * StableFunctionKey  : de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e
 * ExecutionHash      : d466fbd7c311f586c660bf1891bcc9980d671125c47235ba656a6627a6a2cada
 * DebugHash          : 1e4c533c5f2e17b6fb8e76ca82448d30c76521a59d318c7d4520f8cc9653f633
 * EntryAbiHash       : ce69b03c30f4aba54525c218c4551a91f6dfe1f289fefa74e6e6ea01b1bab739
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_d466fbd7c311f586c660bf1891bcc9980d671125c47235ba656a6627a6a2cada_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d14,
	const int32 as_ast_d15,
	const int32 as_ast_d16,
	const int32 as_ast_d17);

static int32 ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_d466fbd7c311f586c660bf1891bcc9980d671125c47235ba656a6627a6a2cada_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d14,
	const int32 as_ast_d15,
	const int32 as_ast_d16,
	const int32 as_ast_d17)
{
	{
		TypedFrame.SetSourcePosition(32, 3);
		switch (as_ast_d17)
		{
			case static_cast<int32>(int32(0)):
				{
					TypedFrame.SetSourcePosition(35, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e16_left = as_ast_d14;
					const int32 as_ast_e16_right = as_ast_d15;
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e16_left, as_ast_e16_right);
				}());
				}
			case static_cast<int32>(int32(1)):
				{
					TypedFrame.SetSourcePosition(37, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e20_left = as_ast_d14;
					const int32 as_ast_e20_right = as_ast_d15;
					return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e20_left, as_ast_e20_right);
				}());
				}
			case static_cast<int32>(int32(2)):
				{
					TypedFrame.SetSourcePosition(39, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e24_left = as_ast_d14;
					const int32 as_ast_e24_right = as_ast_d15;
					return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e24_left, as_ast_e24_right);
				}());
				}
			case static_cast<int32>(int32(3)):
				{
					TypedFrame.SetSourcePosition(41, 4);
					return ([&]() -> int32
				{
					TypedFrame.SetSourcePosition(41, 4);
					const int32 as_ast_e28_left = as_ast_d14;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e28_right = as_ast_d15;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e28_left, as_ast_e28_right);
				}());
				}
			case static_cast<int32>(int32(4)):
				{
					TypedFrame.SetSourcePosition(43, 4);
					return ([&]() -> int32
				{
					TypedFrame.SetSourcePosition(43, 4);
					const int32 as_ast_e32_left = as_ast_d14;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e32_right = as_ast_d15;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::CheckedRemainder<int32>(Execution, as_ast_e32_left, as_ast_e32_right);
				}());
				}
			case static_cast<int32>(int32(5)):
				{
					TypedFrame.SetSourcePosition(45, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e36_left = as_ast_d14;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e36_right = as_ast_d16;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::ShiftLeft<int32>(as_ast_e36_left, as_ast_e36_right);
				}());
				}
			case static_cast<int32>(int32(6)):
				{
					TypedFrame.SetSourcePosition(47, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e40_left = as_ast_d14;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e40_right = as_ast_d16;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::LogicalShiftRight<int32>(as_ast_e40_left, as_ast_e40_right);
				}());
				}
			case static_cast<int32>(int32(7)):
				{
					TypedFrame.SetSourcePosition(49, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e44_left = as_ast_d14;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e44_right = as_ast_d16;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::ArithmeticShiftRight<int32>(as_ast_e44_left, as_ast_e44_right);
				}());
				}
			case static_cast<int32>(int32(8)):
				{
					int64 as_ast_d18;
					{
						TypedFrame.SetSourcePosition(52, 10);
						([&]() -> int64
						{
							const int64 as_ast_e49_value = static_cast<int64>(as_ast_d14);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int64{};
							}
							int64& as_ast_e49_target = as_ast_d18;
							as_ast_e49_target = as_ast_e49_value;
							return as_ast_e49_target;
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(53, 4);
						const bool as_ast_stmt_29_condition = ([&]() -> bool
						{
							const int64 as_ast_e53_left = as_ast_d18;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int64 as_ast_e53_right = static_cast<int64>(as_ast_d14);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e53_left == as_ast_e53_right);
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
						if (as_ast_stmt_29_condition)
						{
							{
								TypedFrame.SetSourcePosition(55, 5);
								return as_ast_d14;
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(57, 4);
						return int32(0);
					}
				}
			case static_cast<int32>(int32(9)):
				{
					TypedFrame.SetSourcePosition(60, 4);
					const bool as_ast_stmt_35_condition = ([&]() -> bool
					{
						const int32 as_ast_e59_left = as_ast_d14;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						const int32 as_ast_e59_right = int32(0);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						return (as_ast_e59_left != as_ast_e59_right);
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return {};
					}
					if (as_ast_stmt_35_condition)
					{
						{
							TypedFrame.SetSourcePosition(62, 5);
							return int32(1);
						}
					}
				}
				{
					TypedFrame.SetSourcePosition(64, 4);
					return int32(0);
				}
			case static_cast<int32>(int32(10)):
				{
					TypedFrame.SetSourcePosition(66, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e65_left = AngelscriptTypedASTJIT::FromCanonicalBits<int32>(UINT64_C(7001));
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e65_right = as_ast_d14;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e65_left, as_ast_e65_right);
				}());
				}
			default:
				{
					TypedFrame.SetSourcePosition(68, 4);
					return AngelscriptTypedASTJIT::WrapNegate<int32>(int32(1));
				}
		}
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
// JIT Entry   : Raw
int32 ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_d466fbd7c311f586c660bf1891bcc9980d671125c47235ba656a6627a6a2cada_Raw(FScriptExecution& Execution, void* Object, asDWORD p_0, asDWORD p_1, asDWORD p_2, asDWORD p_3)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as",
		"int SemanticNumericBoundary(const int, const int, const int, const int)",
		26,
		Object,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_d466fbd7c311f586c660bf1891bcc9980d671125c47235ba656a6627a6a2cada_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1), static_cast<int32>(p_2), static_cast<int32>(p_3));
}

// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
// JIT Entry   : VM
void ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_d466fbd7c311f586c660bf1891bcc9980d671125c47235ba656a6627a6a2cada_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_d466fbd7c311f586c660bf1891bcc9980d671125c47235ba656a6627a6a2cada_Raw(Execution,
		*(void**)l_fp,
		*(asDWORD*)(l_fp + 2),
		*(asDWORD*)(l_fp + 3),
		*(asDWORD*)(l_fp + 4),
		*(asDWORD*)(l_fp + 5));
}

// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
// JIT Entry   : Parms
void ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_d466fbd7c311f586c660bf1891bcc9980d671125c47235ba656a6627a6a2cada_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_2 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_3 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_d466fbd7c311f586c660bf1891bcc9980d671125c47235ba656a6627a6a2cada_Raw(Execution,
		Object,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_2)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_3)));
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
