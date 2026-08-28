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
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:10
 * StableFunctionKey  : 23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67
 * ExecutionHash      : b949dc71a767ba38728abfed5206f2c8c6158a7a0a8f4605d6d47f473c3e3ffa
 * DebugHash          : 798475a1356cd9ede000e9e257db5305fb567fcc5b26c13d548682ff2b212846
 * EntryAbiHash       : 5bdf2107db2403a739f0c1ec988a215ef8d4496714c3a85248de4057037ca025
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static double ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_b949dc71a767ba38728abfed5206f2c8c6158a7a0a8f4605d6d47f473c3e3ffa_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const double as_ast_d25,
	const double as_ast_d26);

static double ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_b949dc71a767ba38728abfed5206f2c8c6158a7a0a8f4605d6d47f473c3e3ffa_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const double as_ast_d25,
	const double as_ast_d26)
{
	{
		TypedFrame.SetSourcePosition(77, 3);
		return ([&]() -> double
	{
		TypedFrame.SetSourcePosition(77, 3);
		const double as_ast_e74_left = as_ast_d25;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		const double as_ast_e74_right = as_ast_d26;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		return AngelscriptTypedASTJIT::CheckedPower<double>(Execution, as_ast_e74_left, as_ast_e74_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return double{};
}

// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:10
// JIT Entry   : Raw
double ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_b949dc71a767ba38728abfed5206f2c8c6158a7a0a8f4605d6d47f473c3e3ffa_Raw(FScriptExecution& Execution, void* Object, double p_0, double p_1)
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
	return ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_b949dc71a767ba38728abfed5206f2c8c6158a7a0a8f4605d6d47f473c3e3ffa_TypedBody(Execution, TypedFrame, static_cast<double>(p_0), static_cast<double>(p_1));
}

// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:10
// JIT Entry   : VM
void ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_b949dc71a767ba38728abfed5206f2c8c6158a7a0a8f4605d6d47f473c3e3ffa_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(double*)l_outValue = ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_b949dc71a767ba38728abfed5206f2c8c6158a7a0a8f4605d6d47f473c3e3ffa_Raw(Execution,
		*(void**)l_fp,
		*(double*)(l_fp + 2),
		*(double*)(l_fp + 4));
}

// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:10
// JIT Entry   : Parms
void ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_b949dc71a767ba38728abfed5206f2c8c6158a7a0a8f4605d6d47f473c3e3ffa_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(double*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_b949dc71a767ba38728abfed5206f2c8c6158a7a0a8f4605d6d47f473c3e3ffa_Raw(Execution,
		Object,
		static_cast<double>(*(double*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<double>(*(double*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UTypedASTFinalScalarReceiver UTypedASTFinalScalarReceiver()
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
 * StableFunctionKey  : b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f
 * ExecutionHash      : 8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e
 * DebugHash          : 9e806183b59b21fa7948787704269e5f2d920918596f4792fc3ae38e9b29cbe6
 * EntryAbiHash       : 5d9f493ad48c6524ca33a73c311249bdf7a4315a86649957cedb5fedfe5acda5
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e = "ASStaticJITTypedFinalScalarFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e
#endif

// AS Function : UTypedASTFinalScalarReceiver UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
// JIT Entry   : Raw
UObject* ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e(FScriptExecution& Execution)
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
UObject* v_TEMP_ptr_2 = nullptr;
// JitEntry *
// PSF v2
// ALLOC
{
  asCObjectType* objType = (asCObjectType*)FAngelscriptJITGeneratedReferenceAccess::GetTypeInfo(Execution, 0u);
  asDWORD* mem = (asDWORD*)SCRIPT_ENGINE->AllocScriptObject(objType);
  ScriptObject_Construct(objType, (asCScriptObject*)mem);
  asPWORD* a = (asPWORD*)((&v_TEMP_ptr_2));
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
l_objectRegister = (void*)v_TEMP_ptr_2;
v_TEMP_ptr_2 = nullptr;
// RET 0
  return (UObject*)l_objectRegister;
}
// AS Function : UTypedASTFinalScalarReceiver UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
// JIT Entry   : VM
void ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(UObject**)l_outValue = ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e(Execution);
}
// AS Function : UTypedASTFinalScalarReceiver UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
// JIT Entry   : Parms
void ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(UObject**)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_b151d3ee1bae6ef99ba4c4ede361b6c537b3764b6d0b663d59032e9069386a7f_8d59a3666cb1e9468ba047e44d12e822bc32d189c66e2841f3e0d28c530e714e(Execution);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int DifferentialScalarValue(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:6
 * StableFunctionKey  : bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b
 * ExecutionHash      : df81f198ae9cc8880f4b4e416ec96091ac1043eb47e016199fc5636cc2f4cac1
 * DebugHash          : 4d5aca58287100e22b66d35028d96ff3e762893097e1f897fd643f8d6f8f3d94
 * EntryAbiHash       : dabb76ccb243078a57121d4c3a40ed732a70a68cca7584d9e9320a1fd5a679c4
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_df81f198ae9cc8880f4b4e416ec96091ac1043eb47e016199fc5636cc2f4cac1_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d9,
	const int32 as_ast_d10);

static int32 ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_df81f198ae9cc8880f4b4e416ec96091ac1043eb47e016199fc5636cc2f4cac1_TypedBody(
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
		const int32 as_ast_e7_left = ([&]() -> int32
	{
		const int32 as_ast_e6_left = as_ast_d9;
		const int32 as_ast_e6_right = int32(3);
		return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e6_left, as_ast_e6_right);
	}());
		const int32 as_ast_e7_right = as_ast_d10;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e7_left, as_ast_e7_right);
	}());
		const int32 as_ast_e9_right = int32(7);
		return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e9_left, as_ast_e9_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int DifferentialScalarValue(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:6
// JIT Entry   : Raw
int32 ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_df81f198ae9cc8880f4b4e416ec96091ac1043eb47e016199fc5636cc2f4cac1_Raw(FScriptExecution& Execution, void* Object, asDWORD p_0, asDWORD p_1)
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
	return ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_df81f198ae9cc8880f4b4e416ec96091ac1043eb47e016199fc5636cc2f4cac1_TypedBody(TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int DifferentialScalarValue(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:6
// JIT Entry   : VM
void ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_df81f198ae9cc8880f4b4e416ec96091ac1043eb47e016199fc5636cc2f4cac1_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_df81f198ae9cc8880f4b4e416ec96091ac1043eb47e016199fc5636cc2f4cac1_Raw(Execution,
		*(void**)l_fp,
		*(asDWORD*)(l_fp + 2),
		*(asDWORD*)(l_fp + 3));
}

// AS Function : int DifferentialScalarValue(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:6
// JIT Entry   : Parms
void ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_df81f198ae9cc8880f4b4e416ec96091ac1043eb47e016199fc5636cc2f4cac1_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_df81f198ae9cc8880f4b4e416ec96091ac1043eb47e016199fc5636cc2f4cac1_Raw(Execution,
		Object,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticImportedBinding(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:6
 * StableFunctionKey  : cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746
 * ExecutionHash      : 571b44cfe995bce53fbc40b6fb7a3df0270ee0f16929ff8cdf4b210750cadc11
 * DebugHash          : f9df30ed67cf7dc64dba01e60da535b225526f86b570bca5928d82a88bba5059
 * EntryAbiHash       : a4b711dd5134aeaca25a93796be749414190495c109eeba3fb1015c75b1a90b1
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_571b44cfe995bce53fbc40b6fb7a3df0270ee0f16929ff8cdf4b210750cadc11 = "ASStaticJITTypedFinalScalarFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_571b44cfe995bce53fbc40b6fb7a3df0270ee0f16929ff8cdf4b210750cadc11
#endif

// AS Function : int SemanticImportedBinding(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:6
// JIT Entry   : Raw
int32 ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_571b44cfe995bce53fbc40b6fb7a3df0270ee0f16929ff8cdf4b210750cadc11(FScriptExecution& Execution, UObject* l_This, asDWORD p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:6
// JIT Entry   : VM
void ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_571b44cfe995bce53fbc40b6fb7a3df0270ee0f16929ff8cdf4b210750cadc11_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_571b44cfe995bce53fbc40b6fb7a3df0270ee0f16929ff8cdf4b210750cadc11(Execution,
		*(UObject**)l_fp,
		*(asDWORD*)(l_fp + 2));
}
// AS Function : int SemanticImportedBinding(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:20:6
// JIT Entry   : Parms
void ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_571b44cfe995bce53fbc40b6fb7a3df0270ee0f16929ff8cdf4b210750cadc11_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_cc438c3e7ddfcc3025b48910f03aa06f782d90388781b3b7ac3ef3303476b746_571b44cfe995bce53fbc40b6fb7a3df0270ee0f16929ff8cdf4b210750cadc11(Execution,
		(UObject*)Object,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : UTypedASTFinalScalarReceiver()
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
 * StableFunctionKey  : d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609
 * ExecutionHash      : 9b5cadc02a8150e77e8b05afd6581c3792b55e11ed5e8da6b462659ba136d4a6
 * DebugHash          : 9e806183b59b21fa7948787704269e5f2d920918596f4792fc3ae38e9b29cbe6
 * EntryAbiHash       : e7fe3e49a0111396d17b5901011b806a76bd6f4b076fedbb28cc2e4f38d7d5b5
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_9b5cadc02a8150e77e8b05afd6581c3792b55e11ed5e8da6b462659ba136d4a6 = "ASStaticJITTypedFinalScalarFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_9b5cadc02a8150e77e8b05afd6581c3792b55e11ed5e8da6b462659ba136d4a6
#endif

// AS Function : UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
// JIT Entry   : Raw
void ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_9b5cadc02a8150e77e8b05afd6581c3792b55e11ed5e8da6b462659ba136d4a6(FScriptExecution& Execution, UObject* l_This)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME_UOBJECT("UTypedASTFinalScalarReceiver::UTypedASTFinalScalarReceiver()", 0);
SCRIPT_ASSUME_NO_EXCEPTION()
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
// JitEntry *
// RET 2
  return;
}
// AS Function : UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
// JIT Entry   : VM
void ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_9b5cadc02a8150e77e8b05afd6581c3792b55e11ed5e8da6b462659ba136d4a6_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_9b5cadc02a8150e77e8b05afd6581c3792b55e11ed5e8da6b462659ba136d4a6(Execution,
		*(UObject**)l_fp);
}
// AS Function : UTypedASTFinalScalarReceiver()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:6:7
// JIT Entry   : Parms
void ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_9b5cadc02a8150e77e8b05afd6581c3792b55e11ed5e8da6b462659ba136d4a6_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	ASJIT_d0a9afe3c2a8d4f8b63ca637092ad281bf29da9fbd0dc92708b4581525261609_9b5cadc02a8150e77e8b05afd6581c3792b55e11ed5e8da6b462659ba136d4a6(Execution,
		(UObject*)Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedFinalRawValue()
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:8:6
 * StableFunctionKey  : d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0
 * ExecutionHash      : c8c1425cade085f454a79c9598d55214cac92d828e3c6e9a34406c982c52ea38
 * DebugHash          : 034c58080e90afd85f8101634662eb22258cd81be638c6dfdfdf4b5cd2259b39
 * EntryAbiHash       : 855bd98169313400b8f1d6686e5875bbad13825c3cc12fdde4e9ad4afcdbc31f
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_c8c1425cade085f454a79c9598d55214cac92d828e3c6e9a34406c982c52ea38_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame);

static int32 ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_c8c1425cade085f454a79c9598d55214cac92d828e3c6e9a34406c982c52ea38_TypedBody(
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
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:8:6
// JIT Entry   : Raw
int32 ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_c8c1425cade085f454a79c9598d55214cac92d828e3c6e9a34406c982c52ea38_Raw(FScriptExecution& Execution, void* Object)
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
	return ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_c8c1425cade085f454a79c9598d55214cac92d828e3c6e9a34406c982c52ea38_TypedBody(TypedFrame);
}

// AS Function : int TypedFinalRawValue()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:8:6
// JIT Entry   : VM
void ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_c8c1425cade085f454a79c9598d55214cac92d828e3c6e9a34406c982c52ea38_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_c8c1425cade085f454a79c9598d55214cac92d828e3c6e9a34406c982c52ea38_Raw(Execution,
		*(void**)l_fp);
}

// AS Function : int TypedFinalRawValue()
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:8:6
// JIT Entry   : Parms
void ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_c8c1425cade085f454a79c9598d55214cac92d828e3c6e9a34406c982c52ea38_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_d74f30e1ae71314f26368c240d19def1d94ff8cc64de8b012050f846e9cf5dd0_c8c1425cade085f454a79c9598d55214cac92d828e3c6e9a34406c982c52ea38_Raw(Execution,
		Object);
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticNumericBoundary(const int, const int, const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:6
 * StableFunctionKey  : de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e
 * ExecutionHash      : e0fe8c2cfcfada2c40ea089f18c78e9feb710c6195f0711860b0af886352893c
 * DebugHash          : 298092bc25472d52eb973f769b952fdeb6d5acb420383e9766f3aad735d13526
 * EntryAbiHash       : ce69b03c30f4aba54525c218c4551a91f6dfe1f289fefa74e6e6ea01b1bab739
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_e0fe8c2cfcfada2c40ea089f18c78e9feb710c6195f0711860b0af886352893c_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d17,
	const int32 as_ast_d18,
	const int32 as_ast_d19,
	const int32 as_ast_d20);

static int32 ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_e0fe8c2cfcfada2c40ea089f18c78e9feb710c6195f0711860b0af886352893c_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d17,
	const int32 as_ast_d18,
	const int32 as_ast_d19,
	const int32 as_ast_d20)
{
	{
		TypedFrame.SetSourcePosition(32, 3);
		switch (as_ast_d20)
		{
			case int32(0):
				{
					TypedFrame.SetSourcePosition(35, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e17_left = as_ast_d17;
					const int32 as_ast_e17_right = as_ast_d18;
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e17_left, as_ast_e17_right);
				}());
				}
			case int32(1):
				{
					TypedFrame.SetSourcePosition(37, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e21_left = as_ast_d17;
					const int32 as_ast_e21_right = as_ast_d18;
					return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e21_left, as_ast_e21_right);
				}());
				}
			case int32(2):
				{
					TypedFrame.SetSourcePosition(39, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e25_left = as_ast_d17;
					const int32 as_ast_e25_right = as_ast_d18;
					return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e25_left, as_ast_e25_right);
				}());
				}
			case int32(3):
				{
					TypedFrame.SetSourcePosition(41, 4);
					return ([&]() -> int32
				{
					TypedFrame.SetSourcePosition(41, 4);
					const int32 as_ast_e29_left = as_ast_d17;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e29_right = as_ast_d18;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e29_left, as_ast_e29_right);
				}());
				}
			case int32(4):
				{
					TypedFrame.SetSourcePosition(43, 4);
					return ([&]() -> int32
				{
					TypedFrame.SetSourcePosition(43, 4);
					const int32 as_ast_e33_left = as_ast_d17;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e33_right = as_ast_d18;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::CheckedRemainder<int32>(Execution, as_ast_e33_left, as_ast_e33_right);
				}());
				}
			case int32(5):
				{
					TypedFrame.SetSourcePosition(45, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e37_left = as_ast_d17;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e37_right = as_ast_d19;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::ShiftLeft<int32>(as_ast_e37_left, as_ast_e37_right);
				}());
				}
			case int32(6):
				{
					TypedFrame.SetSourcePosition(47, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e41_left = as_ast_d17;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e41_right = as_ast_d19;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::LogicalShiftRight<int32>(as_ast_e41_left, as_ast_e41_right);
				}());
				}
			case int32(7):
				{
					TypedFrame.SetSourcePosition(49, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e45_left = as_ast_d17;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e45_right = as_ast_d19;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::ArithmeticShiftRight<int32>(as_ast_e45_left, as_ast_e45_right);
				}());
				}
			case int32(8):
				{
					int64 as_ast_d23;
					{
						TypedFrame.SetSourcePosition(52, 10);
						([&]() -> int64
						{
							const int64 as_ast_e50_value = static_cast<int64>(as_ast_d17);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int64{};
							}
							int64& as_ast_e50_target = as_ast_d23;
							as_ast_e50_target = as_ast_e50_value;
							return as_ast_e50_target;
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
							const int64 as_ast_e54_left = as_ast_d23;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int64 as_ast_e54_right = static_cast<int64>(as_ast_d17);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e54_left == as_ast_e54_right);
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
						if (as_ast_stmt_29_condition)
						{
							{
								TypedFrame.SetSourcePosition(55, 5);
								return as_ast_d17;
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(57, 4);
						return int32(0);
					}
				}
			case int32(9):
				{
					TypedFrame.SetSourcePosition(60, 4);
					const bool as_ast_stmt_35_condition = ([&]() -> bool
					{
						const int32 as_ast_e60_left = as_ast_d17;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						const int32 as_ast_e60_right = int32(0);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						return (as_ast_e60_left != as_ast_e60_right);
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
			case int32(10):
				{
					TypedFrame.SetSourcePosition(66, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e68_left = AngelscriptTypedASTJIT::FromCanonicalBits<int32>(UINT64_C(7001));
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e68_right = as_ast_d17;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e68_left, as_ast_e68_right);
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
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:6
// JIT Entry   : Raw
int32 ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_e0fe8c2cfcfada2c40ea089f18c78e9feb710c6195f0711860b0af886352893c_Raw(FScriptExecution& Execution, void* Object, asDWORD p_0, asDWORD p_1, asDWORD p_2, asDWORD p_3)
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
	return ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_e0fe8c2cfcfada2c40ea089f18c78e9feb710c6195f0711860b0af886352893c_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1), static_cast<int32>(p_2), static_cast<int32>(p_3));
}

// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:6
// JIT Entry   : VM
void ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_e0fe8c2cfcfada2c40ea089f18c78e9feb710c6195f0711860b0af886352893c_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_e0fe8c2cfcfada2c40ea089f18c78e9feb710c6195f0711860b0af886352893c_Raw(Execution,
		*(void**)l_fp,
		*(asDWORD*)(l_fp + 2),
		*(asDWORD*)(l_fp + 3),
		*(asDWORD*)(l_fp + 4),
		*(asDWORD*)(l_fp + 5));
}

// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:6
// JIT Entry   : Parms
void ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_e0fe8c2cfcfada2c40ea089f18c78e9feb710c6195f0711860b0af886352893c_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_e0fe8c2cfcfada2c40ea089f18c78e9feb710c6195f0711860b0af886352893c_Raw(Execution,
		Object,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_2)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_3)));
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
