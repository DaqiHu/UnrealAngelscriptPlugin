// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as
 * CanonicalModuleName : ASStaticJITTypedRecursionFixture
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : d19c9e37621a0cc17f805a0553f41936f2c1f716ef5c91f346c4ce5cc8f6b8b5
 * ProviderId          : 7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44
 * ArtifactProfile     : b58efc4c9f40879f0b98bcb54704d1b342ae56948a00e9ddbaa709787226e0c8
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
 * Declaration       : int TypedASTExceptionEntry(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:50:1
 * StableFunctionKey  : 69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb
 * ExecutionHash      : b566ccbace9238fd4965399cda9bd95da29d8663fb901f80b3747f4b8569797d
 * DebugHash          : 36c5e4c6c38aa2bb0aed3322653ddec5eb0d8183af14f8a76fdc60890b1b078b
 * EntryAbiHash       : 0bb8561a88e967bd0b6be6e0c1b70319bb70e04b1ecab786122674ec9f725c03
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_b566ccbace9238fd4965399cda9bd95da29d8663fb901f80b3747f4b8569797d_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1);
static int32 ASJIT_HelperBody_TypedASTExceptionHelper_b235edf227be8a1f_69188dd501ca6ad0(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1);
static int32 ASJIT_Helper_TypedASTExceptionHelper_b235edf227be8a1f_69188dd501ca6ad0(
	FScriptExecution& Execution,
	const int32 as_helper_p0,
	const int32 as_helper_p1);

static const FAngelscriptTypedASTJITBoundCallSite ASJIT_Call_TypedASTProviderPrivateAdd_6b00a476_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb = {
	TEXT("int TypedASTProviderPrivateAdd(int, int)"),
	TEXT("AngelscriptStaticJITAotFixture::TypedASTProviderPrivateAdd"),
	FAngelscriptStableFunctionKey{FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("6b00a476bd57a82fb216373cb0eef27d00c4acb64af32b342c47c9b7dee91aac"))) }},
	FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("304fbe10a2078ab14e2f67d70c35fd3322a0e9ce8b25e1915cebe00e271b6e96"))) },
	1
};

static int32 ASJIT_HelperBody_TypedASTExceptionHelper_b235edf227be8a1f_69188dd501ca6ad0(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1)
{
	TypedFrame.SetSourcePosition(46, 2);
	return ([&]() -> int32
	{
		const int32 as_sem_e2_left = as_sem_s0;
		const int32 as_sem_e2_right = as_sem_s1;
		return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e2_left, as_sem_e2_right);
	}());
}

static int32 ASJIT_Helper_TypedASTExceptionHelper_b235edf227be8a1f_69188dd501ca6ad0(
	FScriptExecution& Execution,
	const int32 as_helper_p0,
	const int32 as_helper_p1)
{
	asIScriptFunction* const TypedFunction =
		FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(
			Execution, 0u);
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		TypedFunction,
		"/Angelscript/Game/ASStaticJITTypedRecursionFixture.as",
		"int TypedASTExceptionHelper(const int, const int)",
		44,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_HelperBody_TypedASTExceptionHelper_b235edf227be8a1f_69188dd501ca6ad0(
		Execution, TypedFrame, as_helper_p0, as_helper_p1);
}

/*
 * AngelScript Native Call Site 0
 * AS Declaration      : int TypedASTProviderPrivateAdd(int, int)
 * Emitted C++ Callee  : AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>
 * Route               : CurrentNativeBinding
 * Effective Target    : AngelscriptStaticJITAotFixture::TypedASTProviderPrivateAdd
 * Runtime DLL Core    : AngelscriptTypedASTJIT::ResolveBoundNative
 * Metadata Row        : ASJIT_Call_TypedASTProviderPrivateAdd_6b00a476_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb
 * Reference Slot      : 1
 */
static int32 ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_b566ccbace9238fd4965399cda9bd95da29d8663fb901f80b3747f4b8569797d_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1)
{
	TypedFrame.SetSourcePosition(52, 6);
	const int32 as_sem_s2 = ([&]() -> int32
	{
		TypedFrame.SetSourcePosition(52, 15);
		const int32 as_sem_e2_arg1 = as_sem_s1;
		const int32 as_sem_e2_arg0 = as_sem_s0;
		const int32 as_sem_e2_result = ASJIT_Helper_TypedASTExceptionHelper_b235edf227be8a1f_69188dd501ca6ad0(Execution, as_sem_e2_arg0, as_sem_e2_arg1);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_sem_e2_result;
	}());
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	TypedFrame.SetSourcePosition(53, 2);
	([&]() -> int32
	{
		TypedFrame.SetSourcePosition(53, 2);
		const int32 as_sem_e5_arg1 = int32(2);
		const int32 as_sem_e5_arg0 = int32(1);
		const int32 as_sem_e5_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>(
			Execution, ASJIT_Call_TypedASTProviderPrivateAdd_6b00a476_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb, as_sem_e5_arg0, as_sem_e5_arg1);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_sem_e5_result;
	}());
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	TypedFrame.SetSourcePosition(54, 2);
	return ([&]() -> int32
	{
		const int32 as_sem_e8_left = as_sem_s2;
		const int32 as_sem_e8_right = int32(99);
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e8_left, as_sem_e8_right);
	}());
}

// AS Function : int TypedASTExceptionEntry(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:50:1
// JIT Entry   : Raw
int32 ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_b566ccbace9238fd4965399cda9bd95da29d8663fb901f80b3747f4b8569797d_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedRecursionFixture.as",
		"int TypedASTExceptionEntry(const int, const int)",
		50,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_b566ccbace9238fd4965399cda9bd95da29d8663fb901f80b3747f4b8569797d_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int TypedASTExceptionEntry(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:50:1
// JIT Entry   : VM
void ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_b566ccbace9238fd4965399cda9bd95da29d8663fb901f80b3747f4b8569797d_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_b566ccbace9238fd4965399cda9bd95da29d8663fb901f80b3747f4b8569797d_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int TypedASTExceptionEntry(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:50:1
// JIT Entry   : Parms
void ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_b566ccbace9238fd4965399cda9bd95da29d8663fb901f80b3747f4b8569797d_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_b566ccbace9238fd4965399cda9bd95da29d8663fb901f80b3747f4b8569797d_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTFrameEntry(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:36:1
 * StableFunctionKey  : 837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62
 * ExecutionHash      : 8f546f57002beeecac8101d1c0fd2ed2327afc15a81c3889681a48a26adaf331
 * DebugHash          : 07692adf2053d369f225cf479c93c3b8d7059bc8a64f36ff4f6c15afa6cd5e9f
 * EntryAbiHash       : 9a370f34ced1e1b098f2f4b339f10de3f95ee0620d3cf38fd90049284735ef6e
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_8f546f57002beeecac8101d1c0fd2ed2327afc15a81c3889681a48a26adaf331_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0);
static int32 ASJIT_HelperBody_TypedASTFrameHelper_abfbff5b1d349921_837d3cf600bbfd73(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0);
static int32 ASJIT_Helper_TypedASTFrameHelper_abfbff5b1d349921_837d3cf600bbfd73(
	FScriptExecution& Execution,
	const int32 as_helper_p0);

static const FAngelscriptTypedASTJITBoundCallSite ASJIT_Call_TypedASTObserveFrame_b9dfce83_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62 = {
	TEXT("int TypedASTObserveFrame(int)"),
	TEXT("AngelscriptStaticJITAotFixture::TypedASTObserveFrame"),
	FAngelscriptStableFunctionKey{FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("b9dfce8349f066078db688faeb9fcce492a9552cd0aef127f54cab83379b520a"))) }},
	FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("7dd3aae3e651d873f66c3811cadc5922fed9cd6c0029f9e89432fa6ba3e4837c"))) },
	1
};

/*
 * AngelScript Native Call Site 0
 * AS Declaration      : int TypedASTObserveFrame(int)
 * Emitted C++ Callee  : AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32>
 * Route               : CurrentNativeBinding
 * Effective Target    : AngelscriptStaticJITAotFixture::TypedASTObserveFrame
 * Runtime DLL Core    : AngelscriptTypedASTJIT::ResolveBoundNative
 * Metadata Row        : ASJIT_Call_TypedASTObserveFrame_b9dfce83_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62
 * Reference Slot      : 1
 */
static int32 ASJIT_HelperBody_TypedASTFrameHelper_abfbff5b1d349921_837d3cf600bbfd73(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0)
{
	TypedFrame.SetSourcePosition(32, 2);
	return ([&]() -> int32
	{
		const int32 as_sem_e3_left = ([&]() -> int32
	{
		TypedFrame.SetSourcePosition(32, 9);
		const int32 as_sem_e1_arg0 = int32(2);
		const int32 as_sem_e1_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32>(
			Execution, ASJIT_Call_TypedASTObserveFrame_b9dfce83_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62, as_sem_e1_arg0);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_sem_e1_result;
	}());
		const int32 as_sem_e3_right = as_sem_s0;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e3_left, as_sem_e3_right);
	}());
}

static int32 ASJIT_Helper_TypedASTFrameHelper_abfbff5b1d349921_837d3cf600bbfd73(
	FScriptExecution& Execution,
	const int32 as_helper_p0)
{
	asIScriptFunction* const TypedFunction =
		FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(
			Execution, 0u);
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		TypedFunction,
		"/Angelscript/Game/ASStaticJITTypedRecursionFixture.as",
		"int TypedASTFrameHelper(const int)",
		30,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_HelperBody_TypedASTFrameHelper_abfbff5b1d349921_837d3cf600bbfd73(
		Execution, TypedFrame, as_helper_p0);
}

/*
 * AngelScript Native Call Site 0
 * AS Declaration      : int TypedASTObserveFrame(int)
 * Emitted C++ Callee  : AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32>
 * Route               : CurrentNativeBinding
 * Effective Target    : AngelscriptStaticJITAotFixture::TypedASTObserveFrame
 * Runtime DLL Core    : AngelscriptTypedASTJIT::ResolveBoundNative
 * Metadata Row        : ASJIT_Call_TypedASTObserveFrame_b9dfce83_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62
 * Reference Slot      : 1
 */
/*
 * AngelScript Native Call Site 1
 * AS Declaration      : int TypedASTObserveFrame(int)
 * Emitted C++ Callee  : AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32>
 * Route               : CurrentNativeBinding
 * Effective Target    : AngelscriptStaticJITAotFixture::TypedASTObserveFrame
 * Runtime DLL Core    : AngelscriptTypedASTJIT::ResolveBoundNative
 * Metadata Row        : ASJIT_Call_TypedASTObserveFrame_b9dfce83_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62
 * Reference Slot      : 1
 */
static int32 ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_8f546f57002beeecac8101d1c0fd2ed2327afc15a81c3889681a48a26adaf331_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0)
{
	TypedFrame.SetSourcePosition(38, 6);
	const int32 as_sem_s1 = ([&]() -> int32
	{
		TypedFrame.SetSourcePosition(38, 19);
		const int32 as_sem_e1_arg0 = int32(1);
		const int32 as_sem_e1_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32>(
			Execution, ASJIT_Call_TypedASTObserveFrame_b9dfce83_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62, as_sem_e1_arg0);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_sem_e1_result;
	}());
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	TypedFrame.SetSourcePosition(39, 6);
	const int32 as_sem_s2 = ([&]() -> int32
	{
		TypedFrame.SetSourcePosition(39, 15);
		const int32 as_sem_e3_arg0 = as_sem_s0;
		const int32 as_sem_e3_result = ASJIT_Helper_TypedASTFrameHelper_abfbff5b1d349921_837d3cf600bbfd73(Execution, as_sem_e3_arg0);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_sem_e3_result;
	}());
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	TypedFrame.SetSourcePosition(40, 6);
	const int32 as_sem_s3 = ([&]() -> int32
	{
		TypedFrame.SetSourcePosition(40, 18);
		const int32 as_sem_e5_arg0 = int32(3);
		const int32 as_sem_e5_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32>(
			Execution, ASJIT_Call_TypedASTObserveFrame_b9dfce83_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62, as_sem_e5_arg0);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_sem_e5_result;
	}());
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	TypedFrame.SetSourcePosition(41, 2);
	return ([&]() -> int32
	{
		const int32 as_sem_e10_left = ([&]() -> int32
	{
		const int32 as_sem_e8_left = as_sem_s1;
		const int32 as_sem_e8_right = as_sem_s2;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e8_left, as_sem_e8_right);
	}());
		const int32 as_sem_e10_right = as_sem_s3;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e10_left, as_sem_e10_right);
	}());
}

// AS Function : int TypedASTFrameEntry(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:36:1
// JIT Entry   : Raw
int32 ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_8f546f57002beeecac8101d1c0fd2ed2327afc15a81c3889681a48a26adaf331_Raw(FScriptExecution& Execution, asDWORD p_0)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedRecursionFixture.as",
		"int TypedASTFrameEntry(const int)",
		36,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_8f546f57002beeecac8101d1c0fd2ed2327afc15a81c3889681a48a26adaf331_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int TypedASTFrameEntry(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:36:1
// JIT Entry   : VM
void ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_8f546f57002beeecac8101d1c0fd2ed2327afc15a81c3889681a48a26adaf331_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_8f546f57002beeecac8101d1c0fd2ed2327afc15a81c3889681a48a26adaf331_Raw(Execution,
		*(asDWORD*)(l_fp + 0));
}

// AS Function : int TypedASTFrameEntry(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:36:1
// JIT Entry   : Parms
void ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_8f546f57002beeecac8101d1c0fd2ed2327afc15a81c3889681a48a26adaf331_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_8f546f57002beeecac8101d1c0fd2ed2327afc15a81c3889681a48a26adaf331_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTMutualOdd(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:11:1
 * StableFunctionKey  : 9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78
 * ExecutionHash      : 779579186b6f2e8210c89914729510719cc3f80be2c8ff32b6d254033958dcad
 * DebugHash          : 002a5d2b4b1b39c84879e2b18381c8deb713e703c0082510b974a6faa7d69e30
 * EntryAbiHash       : bf59be51292578d00c3cd680f28bc0c167217588050f08dd410162b09320888d
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_779579186b6f2e8210c89914729510719cc3f80be2c8ff32b6d254033958dcad = "ASStaticJITTypedRecursionFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_779579186b6f2e8210c89914729510719cc3f80be2c8ff32b6d254033958dcad
#endif

// AS Function : int TypedASTMutualOdd(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:11:1
// JIT Entry   : Raw
int32 ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_779579186b6f2e8210c89914729510719cc3f80be2c8ff32b6d254033958dcad(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int TypedASTMutualOdd(int)", 13);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[4];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asBYTE v_TEMP_byte_2 = {};
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// CMPIi v0, 0
  if (((int32&)p_Value) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)p_Value) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNP 24
if(((int8&)l_byteRegister) <= 0) {
goto LABEL_TypedASTMutualOdd_35;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SUBIi v1, v0, 1
((int32&)v_TEMP_dword_1) = ((int32&)p_Value) - value_as<int>((asDWORD)0x1u);
// PshV4 v1
// CALL
// int TypedASTMutualEven(const int Value)
SCRIPT_DEBUG_CALLSTACK_POSITION(15, 3);
{
  asCScriptFunction* CallFunction = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 0u);
  FStaticJITCurrentFunctionCall CurrentFunctionCall(Execution, FAngelscriptStableFunctionKey{{FAngelscriptHash256{{FBlake3Hash(FWideStringView(TEXT("f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce")))}}}}, CallFunction, nullptr, false);
  CallFunction = CurrentFunctionCall.Get();
  if (CallFunction == nullptr) [[unlikely]]
  {
     SCRIPT_UNBOUND_EXCEPTION();
return {};
  }
  FAngelscriptContext CallContext(CallFunction->GetEngine());
  CallContext->Prepare(CallFunction);
  CallContext->SetArgDWord(0, value_as<asDWORD>((v_TEMP_dword_1)));
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
// CpyRtoV4 v3
memcpy((&v_TEMP_dword_3), &l_valueRegister, 4);
// ADDIi v1, v3, 1
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_3) + value_as<int>((asDWORD)0x1u);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// JMP 7
goto LABEL_TypedASTMutualOdd_42;
LABEL_TypedASTMutualOdd_35:
// SUSPEND
// JitEntry *
// SetV4 v4, 100
v_TEMP_dword_4 = 0x64u;
// CpyVtoR4 v4
l_dwordRegister = v_TEMP_dword_4;
LABEL_TypedASTMutualOdd_42:
// RET 1
  return (int32)l_dwordRegister;
}
// AS Function : int TypedASTMutualOdd(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:11:1
// JIT Entry   : VM
void ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_779579186b6f2e8210c89914729510719cc3f80be2c8ff32b6d254033958dcad_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_779579186b6f2e8210c89914729510719cc3f80be2c8ff32b6d254033958dcad(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int TypedASTMutualOdd(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:11:1
// JIT Entry   : Parms
void ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_779579186b6f2e8210c89914729510719cc3f80be2c8ff32b6d254033958dcad_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_779579186b6f2e8210c89914729510719cc3f80be2c8ff32b6d254033958dcad(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTFrameHelper(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:30:1
 * StableFunctionKey  : abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd
 * ExecutionHash      : c776e2b663841326350de7154fe3ed6c647b3e38acafdcf47638049faaee0816
 * DebugHash          : 66cf1836d74e06aec1737331fb81b4ea3198b8a728dffd5eac620592779b5097
 * EntryAbiHash       : d445a389231e20b52c85114adb522d9422b08dc0a58b16efe7f59cb9513ee324
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_c776e2b663841326350de7154fe3ed6c647b3e38acafdcf47638049faaee0816 = "ASStaticJITTypedRecursionFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_c776e2b663841326350de7154fe3ed6c647b3e38acafdcf47638049faaee0816
#endif

// AS Function : int TypedASTFrameHelper(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:30:1
// JIT Entry   : Raw
int32 ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_c776e2b663841326350de7154fe3ed6c647b3e38acafdcf47638049faaee0816(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int TypedASTFrameHelper(int)", 32);
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
// PshC4 2
// CALLSYS *
// int TypedASTObserveFrame(int Marker)
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 0u).GetFunction();
  auto CastedFuncPtr = (int32(*)(int32))RawFuncPtr;
SCRIPT_DEBUG_CALLSTACK_POSITION(32, 2);
int32 FunctionReturnValue = CastedFuncPtr(value_as<int32>(((asDWORD)0x2u)));
l_dwordRegister = (asDWORD&)FunctionReturnValue;
if (Execution.bExceptionThrown) [[unlikely]]
{
return {};
}
}
// JitEntry *
// CpyRtoV4 v2
v_TEMP_dword_2 = l_dwordRegister;
// ADDi v1, v2, v0
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_2) + ((int32&)p_Value);
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 1
  return (int32)l_dwordRegister;
}
// AS Function : int TypedASTFrameHelper(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:30:1
// JIT Entry   : VM
void ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_c776e2b663841326350de7154fe3ed6c647b3e38acafdcf47638049faaee0816_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_c776e2b663841326350de7154fe3ed6c647b3e38acafdcf47638049faaee0816(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int TypedASTFrameHelper(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:30:1
// JIT Entry   : Parms
void ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_c776e2b663841326350de7154fe3ed6c647b3e38acafdcf47638049faaee0816_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_c776e2b663841326350de7154fe3ed6c647b3e38acafdcf47638049faaee0816(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTExceptionHelper(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:44:1
 * StableFunctionKey  : b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381
 * ExecutionHash      : 0f9a39a00703748131e087eaf9890b64818033e40d6d74e5c70b5c3d85605c04
 * DebugHash          : 8493f052e587d24972b501354759a3068dbfb4205d6668121a8ad11b6f39a26e
 * EntryAbiHash       : 8a222551660204d8aa9ac9261fac2964b81809e1be0ba8355d19dccb11dd9971
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_0f9a39a00703748131e087eaf9890b64818033e40d6d74e5c70b5c3d85605c04 = "ASStaticJITTypedRecursionFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_0f9a39a00703748131e087eaf9890b64818033e40d6d74e5c70b5c3d85605c04
#endif

// AS Function : int TypedASTExceptionHelper(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:44:1
// JIT Entry   : Raw
int32 ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_0f9a39a00703748131e087eaf9890b64818033e40d6d74e5c70b5c3d85605c04(FScriptExecution& Execution, asDWORD p_Numerator, asDWORD p_Denominator)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int TypedASTExceptionHelper(int, int)", 46);
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
SCRIPT_DEBUG_CALLSTACK_POSITION(46, 2);
     SCRIPT_DIV0_EXCEPTION();
return {};
  }
  else if (divider == -1 && operand == int(0x80000000)) [[unlikely]]
  {
SCRIPT_DEBUG_CALLSTACK_POSITION(46, 2);
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
// AS Function : int TypedASTExceptionHelper(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:44:1
// JIT Entry   : VM
void ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_0f9a39a00703748131e087eaf9890b64818033e40d6d74e5c70b5c3d85605c04_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_0f9a39a00703748131e087eaf9890b64818033e40d6d74e5c70b5c3d85605c04(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}
// AS Function : int TypedASTExceptionHelper(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:44:1
// JIT Entry   : Parms
void ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_0f9a39a00703748131e087eaf9890b64818033e40d6d74e5c70b5c3d85605c04_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Numerator = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_Denominator = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_0f9a39a00703748131e087eaf9890b64818033e40d6d74e5c70b5c3d85605c04(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Numerator),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_Denominator));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTSelfRecursion(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:2:1
 * StableFunctionKey  : efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733
 * ExecutionHash      : 91d2efd59a18658bebbba425b8d1b2e680d7cf9445b9afdf7ff20d7a3ca4a16d
 * DebugHash          : 6a33eab70d8ce206db713ec5d7b1883d84a32a2f206822487c6da8ade97b6d9c
 * EntryAbiHash       : 6d11fd1ead83143bf8f550656196082956507d4c82dc84aeb65e3e9c5bde4cb5
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_91d2efd59a18658bebbba425b8d1b2e680d7cf9445b9afdf7ff20d7a3ca4a16d_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0);
static int32 ASJIT_Helper_TypedASTSelfRecursion_efe75e2004503ee7_efe75e2004503ee7(
	FScriptExecution& Execution,
	const int32 as_helper_p0);

static int32 ASJIT_Helper_TypedASTSelfRecursion_efe75e2004503ee7_efe75e2004503ee7(
	FScriptExecution& Execution,
	const int32 as_helper_p0)
{
	asIScriptFunction* const TypedFunction =
		FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(
			Execution, 0u);
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		TypedFunction,
		"/Angelscript/Game/ASStaticJITTypedRecursionFixture.as",
		"int TypedASTSelfRecursion(const int)",
		2,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_91d2efd59a18658bebbba425b8d1b2e680d7cf9445b9afdf7ff20d7a3ca4a16d_TypedBody(
		Execution, TypedFrame, as_helper_p0);
}

static int32 ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_91d2efd59a18658bebbba425b8d1b2e680d7cf9445b9afdf7ff20d7a3ca4a16d_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0)
{
	TypedFrame.SetSourcePosition(4, 2);
	const bool as_sem_if_condition_2 = ([&]() -> bool
	{
		const int32 as_sem_e2_left = as_sem_s0;
		const int32 as_sem_e2_right = int32(0);
		return (as_sem_e2_left > as_sem_e2_right);
	}());
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	if (as_sem_if_condition_2)
	{
		TypedFrame.SetSourcePosition(6, 3);
		return ([&]() -> int32
		{
			const int32 as_sem_e8_left = as_sem_s0;
			const int32 as_sem_e8_right = ([&]() -> int32
		{
			TypedFrame.SetSourcePosition(6, 18);
			const int32 as_sem_e7_arg0 = ([&]() -> int32
			{
				const int32 as_sem_e6_left = as_sem_s0;
				const int32 as_sem_e6_right = int32(1);
				return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_sem_e6_left, as_sem_e6_right);
			}());
			const int32 as_sem_e7_result = ASJIT_Helper_TypedASTSelfRecursion_efe75e2004503ee7_efe75e2004503ee7(Execution, as_sem_e7_arg0);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			return as_sem_e7_result;
		}());
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e8_left, as_sem_e8_right);
		}());
	}
	TypedFrame.SetSourcePosition(8, 2);
	return int32(1);
}

// AS Function : int TypedASTSelfRecursion(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:2:1
// JIT Entry   : Raw
int32 ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_91d2efd59a18658bebbba425b8d1b2e680d7cf9445b9afdf7ff20d7a3ca4a16d_Raw(FScriptExecution& Execution, asDWORD p_0)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedRecursionFixture.as",
		"int TypedASTSelfRecursion(const int)",
		2,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_91d2efd59a18658bebbba425b8d1b2e680d7cf9445b9afdf7ff20d7a3ca4a16d_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int TypedASTSelfRecursion(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:2:1
// JIT Entry   : VM
void ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_91d2efd59a18658bebbba425b8d1b2e680d7cf9445b9afdf7ff20d7a3ca4a16d_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_91d2efd59a18658bebbba425b8d1b2e680d7cf9445b9afdf7ff20d7a3ca4a16d_Raw(Execution,
		*(asDWORD*)(l_fp + 0));
}

// AS Function : int TypedASTSelfRecursion(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:2:1
// JIT Entry   : Parms
void ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_91d2efd59a18658bebbba425b8d1b2e680d7cf9445b9afdf7ff20d7a3ca4a16d_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_91d2efd59a18658bebbba425b8d1b2e680d7cf9445b9afdf7ff20d7a3ca4a16d_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTMutualEven(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:21:1
 * StableFunctionKey  : f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce
 * ExecutionHash      : de8a9f27d062164b0e5cd7b867de2654db03486fadca8fb13990539a7afe9788
 * DebugHash          : 88a975ac11b1fcc348e0ac88cd1b3ea7a6aed46b9d9949c6a74f50170621a862
 * EntryAbiHash       : fa5cbe60f891ca77e110682e6c1613af4186ed9e47cbc9d0531d8dde05cd3a5c
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_HelperBody_TypedASTMutualOdd_9b21b2a1cce2fd68_f3cd404bfc2f5d0c(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0);
static int32 ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_de8a9f27d062164b0e5cd7b867de2654db03486fadca8fb13990539a7afe9788_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0);
static int32 ASJIT_Helper_TypedASTMutualOdd_9b21b2a1cce2fd68_f3cd404bfc2f5d0c(
	FScriptExecution& Execution,
	const int32 as_helper_p0);
static int32 ASJIT_Helper_TypedASTMutualEven_f3cd404bfc2f5d0c_f3cd404bfc2f5d0c(
	FScriptExecution& Execution,
	const int32 as_helper_p0);

static int32 ASJIT_HelperBody_TypedASTMutualOdd_9b21b2a1cce2fd68_f3cd404bfc2f5d0c(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0)
{
	TypedFrame.SetSourcePosition(13, 2);
	const bool as_sem_if_condition_2 = ([&]() -> bool
	{
		const int32 as_sem_e2_left = as_sem_s0;
		const int32 as_sem_e2_right = int32(0);
		return (as_sem_e2_left > as_sem_e2_right);
	}());
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	if (as_sem_if_condition_2)
	{
		TypedFrame.SetSourcePosition(15, 3);
		return ([&]() -> int32
		{
			const int32 as_sem_e8_left = ([&]() -> int32
		{
			TypedFrame.SetSourcePosition(15, 10);
			const int32 as_sem_e6_arg0 = ([&]() -> int32
			{
				const int32 as_sem_e5_left = as_sem_s0;
				const int32 as_sem_e5_right = int32(1);
				return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_sem_e5_left, as_sem_e5_right);
			}());
			const int32 as_sem_e6_result = ASJIT_Helper_TypedASTMutualEven_f3cd404bfc2f5d0c_f3cd404bfc2f5d0c(Execution, as_sem_e6_arg0);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			return as_sem_e6_result;
		}());
			const int32 as_sem_e8_right = int32(1);
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e8_left, as_sem_e8_right);
		}());
	}
	TypedFrame.SetSourcePosition(17, 2);
	return int32(100);
}

static int32 ASJIT_Helper_TypedASTMutualOdd_9b21b2a1cce2fd68_f3cd404bfc2f5d0c(
	FScriptExecution& Execution,
	const int32 as_helper_p0)
{
	asIScriptFunction* const TypedFunction =
		FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(
			Execution, 0u);
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		TypedFunction,
		"/Angelscript/Game/ASStaticJITTypedRecursionFixture.as",
		"int TypedASTMutualOdd(const int)",
		11,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_HelperBody_TypedASTMutualOdd_9b21b2a1cce2fd68_f3cd404bfc2f5d0c(
		Execution, TypedFrame, as_helper_p0);
}

static int32 ASJIT_Helper_TypedASTMutualEven_f3cd404bfc2f5d0c_f3cd404bfc2f5d0c(
	FScriptExecution& Execution,
	const int32 as_helper_p0)
{
	asIScriptFunction* const TypedFunction =
		FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(
			Execution, 1u);
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		TypedFunction,
		"/Angelscript/Game/ASStaticJITTypedRecursionFixture.as",
		"int TypedASTMutualEven(const int)",
		21,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_de8a9f27d062164b0e5cd7b867de2654db03486fadca8fb13990539a7afe9788_TypedBody(
		Execution, TypedFrame, as_helper_p0);
}

static int32 ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_de8a9f27d062164b0e5cd7b867de2654db03486fadca8fb13990539a7afe9788_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0)
{
	TypedFrame.SetSourcePosition(23, 2);
	const bool as_sem_if_condition_2 = ([&]() -> bool
	{
		const int32 as_sem_e2_left = as_sem_s0;
		const int32 as_sem_e2_right = int32(0);
		return (as_sem_e2_left > as_sem_e2_right);
	}());
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	if (as_sem_if_condition_2)
	{
		TypedFrame.SetSourcePosition(25, 3);
		return ([&]() -> int32
		{
			const int32 as_sem_e8_left = ([&]() -> int32
		{
			TypedFrame.SetSourcePosition(25, 10);
			const int32 as_sem_e6_arg0 = ([&]() -> int32
			{
				const int32 as_sem_e5_left = as_sem_s0;
				const int32 as_sem_e5_right = int32(1);
				return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_sem_e5_left, as_sem_e5_right);
			}());
			const int32 as_sem_e6_result = ASJIT_Helper_TypedASTMutualOdd_9b21b2a1cce2fd68_f3cd404bfc2f5d0c(Execution, as_sem_e6_arg0);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			return as_sem_e6_result;
		}());
			const int32 as_sem_e8_right = int32(1);
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e8_left, as_sem_e8_right);
		}());
	}
	TypedFrame.SetSourcePosition(27, 2);
	return int32(100);
}

// AS Function : int TypedASTMutualEven(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:21:1
// JIT Entry   : Raw
int32 ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_de8a9f27d062164b0e5cd7b867de2654db03486fadca8fb13990539a7afe9788_Raw(FScriptExecution& Execution, asDWORD p_0)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedRecursionFixture.as",
		"int TypedASTMutualEven(const int)",
		21,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_de8a9f27d062164b0e5cd7b867de2654db03486fadca8fb13990539a7afe9788_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int TypedASTMutualEven(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:21:1
// JIT Entry   : VM
void ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_de8a9f27d062164b0e5cd7b867de2654db03486fadca8fb13990539a7afe9788_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_de8a9f27d062164b0e5cd7b867de2654db03486fadca8fb13990539a7afe9788_Raw(Execution,
		*(asDWORD*)(l_fp + 0));
}

// AS Function : int TypedASTMutualEven(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:21:1
// JIT Entry   : Parms
void ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_de8a9f27d062164b0e5cd7b867de2654db03486fadca8fb13990539a7afe9788_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_de8a9f27d062164b0e5cd7b867de2654db03486fadca8fb13990539a7afe9788_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
