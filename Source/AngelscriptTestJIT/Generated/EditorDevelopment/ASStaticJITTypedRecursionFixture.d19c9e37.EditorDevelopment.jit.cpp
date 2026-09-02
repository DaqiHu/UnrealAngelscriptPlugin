// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as
 * CanonicalModuleName : ASStaticJITTypedRecursionFixture
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : d19c9e37621a0cc17f805a0553f41936f2c1f716ef5c91f346c4ce5cc8f6b8b5
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
 * Declaration       : int TypedASTExceptionEntry(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:50:1
 * StableFunctionKey  : 69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb
 * ExecutionHash      : a428898941774995a8193adc18f385ccc38e35065f6d6d12a46b37b5172e7df8
 * DebugHash          : 601035c3d4873bca1a201abe4e7ef850806d3dad06b58c1f0521bb9214d1c577
 * EntryAbiHash       : 0bb8561a88e967bd0b6be6e0c1b70319bb70e04b1ecab786122674ec9f725c03
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a428898941774995a8193adc18f385ccc38e35065f6d6d12a46b37b5172e7df8_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d21,
	const int32 as_ast_d22);
static int32 ASJIT_HelperBody_TypedASTExceptionHelper_b235edf227be8a1f_69188dd501ca6ad0(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d18,
	const int32 as_ast_d19);
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
	const int32 as_ast_d18,
	const int32 as_ast_d19)
{
	{
		TypedFrame.SetSourcePosition(46, 2);
		return ([&]() -> int32
	{
		TypedFrame.SetSourcePosition(46, 2);
		const int32 as_ast_e54_left = as_ast_d18;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		const int32 as_ast_e54_right = as_ast_d19;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e54_left, as_ast_e54_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
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
static int32 ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a428898941774995a8193adc18f385ccc38e35065f6d6d12a46b37b5172e7df8_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d21,
	const int32 as_ast_d22)
{
	int32 as_ast_d23;
	{
		TypedFrame.SetSourcePosition(52, 6);
		([&]() -> int32
		{
			const int32 as_ast_e59_value = ([&]() -> int32
		{
			const int32 as_ast_e57_arg1 = as_ast_d22;
			const int32 as_ast_e57_arg0 = as_ast_d21;
			const int32 as_ast_e57_result = ASJIT_Helper_TypedASTExceptionHelper_b235edf227be8a1f_69188dd501ca6ad0(Execution, as_ast_e57_arg0, as_ast_e57_arg1);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			return as_ast_e57_result;
		}());
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			int32& as_ast_e59_target = as_ast_d23;
			as_ast_e59_target = as_ast_e59_value;
			return as_ast_e59_target;
		}());
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
	}
	{
		TypedFrame.SetSourcePosition(53, 2);
		([&]() -> int32
		{
			const int32 as_ast_e62_arg1 = int32(2);
			const int32 as_ast_e62_arg0 = int32(1);
			const int32 as_ast_e62_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>(Execution, ASJIT_Call_TypedASTProviderPrivateAdd_6b00a476_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb, as_ast_e62_arg0, as_ast_e62_arg1);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			return as_ast_e62_result;
		}());
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
	}
	{
		TypedFrame.SetSourcePosition(54, 2);
		return ([&]() -> int32
	{
		const int32 as_ast_e65_left = as_ast_d23;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		const int32 as_ast_e65_right = int32(99);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e65_left, as_ast_e65_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int TypedASTExceptionEntry(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:50:1
// JIT Entry   : Raw
int32 ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a428898941774995a8193adc18f385ccc38e35065f6d6d12a46b37b5172e7df8_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
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
	return ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a428898941774995a8193adc18f385ccc38e35065f6d6d12a46b37b5172e7df8_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int TypedASTExceptionEntry(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:50:1
// JIT Entry   : VM
void ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a428898941774995a8193adc18f385ccc38e35065f6d6d12a46b37b5172e7df8_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a428898941774995a8193adc18f385ccc38e35065f6d6d12a46b37b5172e7df8_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int TypedASTExceptionEntry(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:50:1
// JIT Entry   : Parms
void ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a428898941774995a8193adc18f385ccc38e35065f6d6d12a46b37b5172e7df8_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a428898941774995a8193adc18f385ccc38e35065f6d6d12a46b37b5172e7df8_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTFrameEntry(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:36:1
 * StableFunctionKey  : 837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62
 * ExecutionHash      : 1dfcd5bff24544e1a8fd461032a99cf1be633f663b19b70a25c14f0270c7baef
 * DebugHash          : 007deca3f0f5f74ee5f49b02605123934eb96a108e66bbf9b3127fe5fc8470a6
 * EntryAbiHash       : 9a370f34ced1e1b098f2f4b339f10de3f95ee0620d3cf38fd90049284735ef6e
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_1dfcd5bff24544e1a8fd461032a99cf1be633f663b19b70a25c14f0270c7baef_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d13);
static int32 ASJIT_HelperBody_TypedASTFrameHelper_abfbff5b1d349921_837d3cf600bbfd73(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d9);
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
	const int32 as_ast_d9)
{
	{
		TypedFrame.SetSourcePosition(32, 2);
		return ([&]() -> int32
	{
		const int32 as_ast_e34_left = ([&]() -> int32
	{
		const int32 as_ast_e32_arg0 = int32(2);
		const int32 as_ast_e32_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32>(Execution, ASJIT_Call_TypedASTObserveFrame_b9dfce83_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62, as_ast_e32_arg0);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_ast_e32_result;
	}());
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		const int32 as_ast_e34_right = as_ast_d9;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e34_left, as_ast_e34_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
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
static int32 ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_1dfcd5bff24544e1a8fd461032a99cf1be633f663b19b70a25c14f0270c7baef_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d13)
{
	int32 as_ast_d14;
	{
		TypedFrame.SetSourcePosition(38, 6);
		([&]() -> int32
		{
			const int32 as_ast_e38_value = ([&]() -> int32
		{
			const int32 as_ast_e36_arg0 = int32(1);
			const int32 as_ast_e36_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32>(Execution, ASJIT_Call_TypedASTObserveFrame_b9dfce83_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62, as_ast_e36_arg0);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			return as_ast_e36_result;
		}());
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			int32& as_ast_e38_target = as_ast_d14;
			as_ast_e38_target = as_ast_e38_value;
			return as_ast_e38_target;
		}());
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
	}
	int32 as_ast_d15;
	{
		TypedFrame.SetSourcePosition(39, 6);
		([&]() -> int32
		{
			const int32 as_ast_e42_value = ([&]() -> int32
		{
			const int32 as_ast_e40_arg0 = as_ast_d13;
			const int32 as_ast_e40_result = ASJIT_Helper_TypedASTFrameHelper_abfbff5b1d349921_837d3cf600bbfd73(Execution, as_ast_e40_arg0);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			return as_ast_e40_result;
		}());
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			int32& as_ast_e42_target = as_ast_d15;
			as_ast_e42_target = as_ast_e42_value;
			return as_ast_e42_target;
		}());
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
	}
	int32 as_ast_d16;
	{
		TypedFrame.SetSourcePosition(40, 6);
		([&]() -> int32
		{
			const int32 as_ast_e46_value = ([&]() -> int32
		{
			const int32 as_ast_e44_arg0 = int32(3);
			const int32 as_ast_e44_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32>(Execution, ASJIT_Call_TypedASTObserveFrame_b9dfce83_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62, as_ast_e44_arg0);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			return as_ast_e44_result;
		}());
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			int32& as_ast_e46_target = as_ast_d16;
			as_ast_e46_target = as_ast_e46_value;
			return as_ast_e46_target;
		}());
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
	}
	{
		TypedFrame.SetSourcePosition(41, 2);
		return ([&]() -> int32
	{
		const int32 as_ast_e51_left = ([&]() -> int32
	{
		const int32 as_ast_e50_left = as_ast_d14;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		const int32 as_ast_e50_right = as_ast_d15;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e50_left, as_ast_e50_right);
	}());
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		const int32 as_ast_e51_right = as_ast_d16;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e51_left, as_ast_e51_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int TypedASTFrameEntry(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:36:1
// JIT Entry   : Raw
int32 ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_1dfcd5bff24544e1a8fd461032a99cf1be633f663b19b70a25c14f0270c7baef_Raw(FScriptExecution& Execution, asDWORD p_0)
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
	return ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_1dfcd5bff24544e1a8fd461032a99cf1be633f663b19b70a25c14f0270c7baef_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int TypedASTFrameEntry(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:36:1
// JIT Entry   : VM
void ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_1dfcd5bff24544e1a8fd461032a99cf1be633f663b19b70a25c14f0270c7baef_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_1dfcd5bff24544e1a8fd461032a99cf1be633f663b19b70a25c14f0270c7baef_Raw(Execution,
		*(asDWORD*)(l_fp + 0));
}

// AS Function : int TypedASTFrameEntry(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:36:1
// JIT Entry   : Parms
void ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_1dfcd5bff24544e1a8fd461032a99cf1be633f663b19b70a25c14f0270c7baef_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_1dfcd5bff24544e1a8fd461032a99cf1be633f663b19b70a25c14f0270c7baef_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTMutualOdd(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:11:1
 * StableFunctionKey  : 9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78
 * ExecutionHash      : 6088b379ba9c4a374e05aecbd6bf12914e94004a463593bc8aed8a886a635f6e
 * DebugHash          : 99e63d41b7521220817acac41a365614ce02f934d46e4e8130f4ca5e1d349c24
 * EntryAbiHash       : bf59be51292578d00c3cd680f28bc0c167217588050f08dd410162b09320888d
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_6088b379ba9c4a374e05aecbd6bf12914e94004a463593bc8aed8a886a635f6e = "ASStaticJITTypedRecursionFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_6088b379ba9c4a374e05aecbd6bf12914e94004a463593bc8aed8a886a635f6e
#endif

// AS Function : int TypedASTMutualOdd(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:11:1
// JIT Entry   : Raw
int32 ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_6088b379ba9c4a374e05aecbd6bf12914e94004a463593bc8aed8a886a635f6e(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int TypedASTMutualOdd(int)", 12);
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
asBYTE v_TEMP_byte_3 = {};
asDWORD v_TEMP_dword_4 = {};
asDWORD v_TEMP_dword_5 = {};
asDWORD v_TEMP_dword_6 = {};
asDWORD v_TEMP_dword_7 = {};
asDWORD v_TEMP_dword_8 = {};
asDWORD v_TEMP_dword_9 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// CMPIi v0, 0
  if (((int32&)p_Value) == value_as<int>((asDWORD)0x0u))     l_byteRegister = 0;
  else if (((int32&)p_Value) < value_as<int>((asDWORD)0x0u)) l_byteRegister = -1;
  else                l_byteRegister = 1;
// JNP 23
if(((int8&)l_byteRegister) <= 0) {
goto LABEL_TypedASTMutualOdd_38;
}
// SUSPEND
// JitEntry *
// SUSPEND
// JitEntry *
// SUBIi v5, v0, 1
((int32&)v_TEMP_dword_5) = ((int32&)p_Value) - value_as<int>((asDWORD)0x1u);
// PshV4 v5
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
  CallContext->SetArgDWord(0, value_as<asDWORD>((v_TEMP_dword_5)));
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
// CpyRtoV4 v6
memcpy((&v_TEMP_dword_6), &l_valueRegister, 4);
// ADDIi v1, v6, 1
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_6) + value_as<int>((asDWORD)0x1u);
// JMP 8
goto LABEL_TypedASTMutualOdd_46;
LABEL_TypedASTMutualOdd_38:
// SUSPEND
// JitEntry *
// SetV4 v1, 100
v_TEMP_dword_1 = 0x64u;
// JMP 0
goto LABEL_TypedASTMutualOdd_46;
LABEL_TypedASTMutualOdd_46:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 1
  return (int32)l_dwordRegister;
}
// AS Function : int TypedASTMutualOdd(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:11:1
// JIT Entry   : VM
void ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_6088b379ba9c4a374e05aecbd6bf12914e94004a463593bc8aed8a886a635f6e_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_6088b379ba9c4a374e05aecbd6bf12914e94004a463593bc8aed8a886a635f6e(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int TypedASTMutualOdd(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:11:1
// JIT Entry   : Parms
void ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_6088b379ba9c4a374e05aecbd6bf12914e94004a463593bc8aed8a886a635f6e_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_6088b379ba9c4a374e05aecbd6bf12914e94004a463593bc8aed8a886a635f6e(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTFrameHelper(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:30:1
 * StableFunctionKey  : abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd
 * ExecutionHash      : fc9822dac818bce9caaba6a16475105207001105e9870f15efd130ac0cd67338
 * DebugHash          : 3dde0f9bfaaff87d9e8714a5a9ae39380de95749c6e1b25d96faef26cf05865e
 * EntryAbiHash       : d445a389231e20b52c85114adb522d9422b08dc0a58b16efe7f59cb9513ee324
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_fc9822dac818bce9caaba6a16475105207001105e9870f15efd130ac0cd67338 = "ASStaticJITTypedRecursionFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_fc9822dac818bce9caaba6a16475105207001105e9870f15efd130ac0cd67338
#endif

// AS Function : int TypedASTFrameHelper(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:30:1
// JIT Entry   : Raw
int32 ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_fc9822dac818bce9caaba6a16475105207001105e9870f15efd130ac0cd67338(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int TypedASTFrameHelper(int)", 31);
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
asDWORD v_TEMP_dword_3 = {};
asDWORD v_TEMP_dword_4 = {};
// JitEntry *
// SUSPEND
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
// CpyRtoV4 v3
v_TEMP_dword_3 = l_dwordRegister;
// ADDi v1, v3, v0
((int32&)v_TEMP_dword_1) = ((int32&)v_TEMP_dword_3) + ((int32&)p_Value);
// JMP 0
goto LABEL_TypedASTFrameHelper_24;
LABEL_TypedASTFrameHelper_24:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 1
  return (int32)l_dwordRegister;
}
// AS Function : int TypedASTFrameHelper(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:30:1
// JIT Entry   : VM
void ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_fc9822dac818bce9caaba6a16475105207001105e9870f15efd130ac0cd67338_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_fc9822dac818bce9caaba6a16475105207001105e9870f15efd130ac0cd67338(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int TypedASTFrameHelper(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:30:1
// JIT Entry   : Parms
void ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_fc9822dac818bce9caaba6a16475105207001105e9870f15efd130ac0cd67338_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_fc9822dac818bce9caaba6a16475105207001105e9870f15efd130ac0cd67338(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTExceptionHelper(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:44:1
 * StableFunctionKey  : b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381
 * ExecutionHash      : 80c326a07f16dd669454f7903d8c434c1710d339a7dcef53dc8b8a78d16bcb86
 * DebugHash          : 793a46fb263a4991423e5ddcf370cdffb8fed1ecde5a12dc880fe5f38a18ba1c
 * EntryAbiHash       : 8a222551660204d8aa9ac9261fac2964b81809e1be0ba8355d19dccb11dd9971
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_80c326a07f16dd669454f7903d8c434c1710d339a7dcef53dc8b8a78d16bcb86 = "ASStaticJITTypedRecursionFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_80c326a07f16dd669454f7903d8c434c1710d339a7dcef53dc8b8a78d16bcb86
#endif

// AS Function : int TypedASTExceptionHelper(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:44:1
// JIT Entry   : Raw
int32 ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_80c326a07f16dd669454f7903d8c434c1710d339a7dcef53dc8b8a78d16bcb86(FScriptExecution& Execution, asDWORD p_Numerator, asDWORD p_Denominator)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("int TypedASTExceptionHelper(int, int)", 45);
SCRIPT_ASSUME_NO_EXCEPTION()
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
// JMP 0
goto LABEL_TypedASTExceptionHelper_15;
LABEL_TypedASTExceptionHelper_15:
// CpyVtoR4 v1
l_dwordRegister = v_TEMP_dword_1;
// RET 2
  return (int32)l_dwordRegister;
}
// AS Function : int TypedASTExceptionHelper(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:44:1
// JIT Entry   : VM
void ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_80c326a07f16dd669454f7903d8c434c1710d339a7dcef53dc8b8a78d16bcb86_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_80c326a07f16dd669454f7903d8c434c1710d339a7dcef53dc8b8a78d16bcb86(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}
// AS Function : int TypedASTExceptionHelper(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:44:1
// JIT Entry   : Parms
void ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_80c326a07f16dd669454f7903d8c434c1710d339a7dcef53dc8b8a78d16bcb86_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Numerator = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_Denominator = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_80c326a07f16dd669454f7903d8c434c1710d339a7dcef53dc8b8a78d16bcb86(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Numerator),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_Denominator));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTSelfRecursion(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:2:1
 * StableFunctionKey  : efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733
 * ExecutionHash      : da5f3557e54a5d71f12ce4a41c01f85a896528a9d47ca6c09f45ded47c4d59d6
 * DebugHash          : f655a5db28270a40b958876fef3c37bbb836af4ca15204a25c8b7b5a5945bec3
 * EntryAbiHash       : 6d11fd1ead83143bf8f550656196082956507d4c82dc84aeb65e3e9c5bde4cb5
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_da5f3557e54a5d71f12ce4a41c01f85a896528a9d47ca6c09f45ded47c4d59d6_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d3);
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
	return ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_da5f3557e54a5d71f12ce4a41c01f85a896528a9d47ca6c09f45ded47c4d59d6_TypedBody(
		Execution, TypedFrame, as_helper_p0);
}

static int32 ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_da5f3557e54a5d71f12ce4a41c01f85a896528a9d47ca6c09f45ded47c4d59d6_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d3)
{
	{
		TypedFrame.SetSourcePosition(4, 2);
		const bool as_ast_stmt_3_condition = ([&]() -> bool
		{
			const int32 as_ast_e3_left = as_ast_d3;
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return bool{};
			}
			const int32 as_ast_e3_right = int32(0);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return bool{};
			}
			return (as_ast_e3_left > as_ast_e3_right);
		}());
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
		if (as_ast_stmt_3_condition)
		{
			{
				TypedFrame.SetSourcePosition(6, 3);
				return ([&]() -> int32
			{
				const int32 as_ast_e9_left = as_ast_d3;
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				const int32 as_ast_e9_right = ([&]() -> int32
			{
				const int32 as_ast_e8_arg0 = ([&]() -> int32
				{
					const int32 as_ast_e7_left = as_ast_d3;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e7_right = int32(1);
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e7_left, as_ast_e7_right);
				}());
				const int32 as_ast_e8_result = ASJIT_Helper_TypedASTSelfRecursion_efe75e2004503ee7_efe75e2004503ee7(Execution, as_ast_e8_arg0);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				return as_ast_e8_result;
			}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e9_left, as_ast_e9_right);
			}());
			}
		}
	}
	{
		TypedFrame.SetSourcePosition(8, 2);
		return int32(1);
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int TypedASTSelfRecursion(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:2:1
// JIT Entry   : Raw
int32 ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_da5f3557e54a5d71f12ce4a41c01f85a896528a9d47ca6c09f45ded47c4d59d6_Raw(FScriptExecution& Execution, asDWORD p_0)
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
	return ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_da5f3557e54a5d71f12ce4a41c01f85a896528a9d47ca6c09f45ded47c4d59d6_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int TypedASTSelfRecursion(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:2:1
// JIT Entry   : VM
void ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_da5f3557e54a5d71f12ce4a41c01f85a896528a9d47ca6c09f45ded47c4d59d6_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_da5f3557e54a5d71f12ce4a41c01f85a896528a9d47ca6c09f45ded47c4d59d6_Raw(Execution,
		*(asDWORD*)(l_fp + 0));
}

// AS Function : int TypedASTSelfRecursion(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:2:1
// JIT Entry   : Parms
void ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_da5f3557e54a5d71f12ce4a41c01f85a896528a9d47ca6c09f45ded47c4d59d6_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_da5f3557e54a5d71f12ce4a41c01f85a896528a9d47ca6c09f45ded47c4d59d6_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTMutualEven(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:21:1
 * StableFunctionKey  : f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce
 * ExecutionHash      : 8588bac2591fc1969f2fc7fa17b3adab41afb88f6e971dbea48adf0af50f110b
 * DebugHash          : b94558b05ab979953bc9856af173359fae094a1ec30e8c1a168139c76912608c
 * EntryAbiHash       : fa5cbe60f891ca77e110682e6c1613af4186ed9e47cbc9d0531d8dde05cd3a5c
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_HelperBody_TypedASTMutualOdd_9b21b2a1cce2fd68_f3cd404bfc2f5d0c(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d5);
static int32 ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_8588bac2591fc1969f2fc7fa17b3adab41afb88f6e971dbea48adf0af50f110b_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d7);
static int32 ASJIT_Helper_TypedASTMutualOdd_9b21b2a1cce2fd68_f3cd404bfc2f5d0c(
	FScriptExecution& Execution,
	const int32 as_helper_p0);
static int32 ASJIT_Helper_TypedASTMutualEven_f3cd404bfc2f5d0c_f3cd404bfc2f5d0c(
	FScriptExecution& Execution,
	const int32 as_helper_p0);

static int32 ASJIT_HelperBody_TypedASTMutualOdd_9b21b2a1cce2fd68_f3cd404bfc2f5d0c(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d5)
{
	{
		TypedFrame.SetSourcePosition(13, 2);
		const bool as_ast_stmt_8_condition = ([&]() -> bool
		{
			const int32 as_ast_e13_left = as_ast_d5;
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return bool{};
			}
			const int32 as_ast_e13_right = int32(0);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return bool{};
			}
			return (as_ast_e13_left > as_ast_e13_right);
		}());
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
		if (as_ast_stmt_8_condition)
		{
			{
				TypedFrame.SetSourcePosition(15, 3);
				return ([&]() -> int32
			{
				const int32 as_ast_e19_left = ([&]() -> int32
			{
				const int32 as_ast_e17_arg0 = ([&]() -> int32
				{
					const int32 as_ast_e16_left = as_ast_d5;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e16_right = int32(1);
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e16_left, as_ast_e16_right);
				}());
				const int32 as_ast_e17_result = ASJIT_Helper_TypedASTMutualEven_f3cd404bfc2f5d0c_f3cd404bfc2f5d0c(Execution, as_ast_e17_arg0);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				return as_ast_e17_result;
			}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				const int32 as_ast_e19_right = int32(1);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e19_left, as_ast_e19_right);
			}());
			}
		}
	}
	{
		TypedFrame.SetSourcePosition(17, 2);
		return int32(100);
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
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
	return ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_8588bac2591fc1969f2fc7fa17b3adab41afb88f6e971dbea48adf0af50f110b_TypedBody(
		Execution, TypedFrame, as_helper_p0);
}

static int32 ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_8588bac2591fc1969f2fc7fa17b3adab41afb88f6e971dbea48adf0af50f110b_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d7)
{
	{
		TypedFrame.SetSourcePosition(23, 2);
		const bool as_ast_stmt_13_condition = ([&]() -> bool
		{
			const int32 as_ast_e23_left = as_ast_d7;
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return bool{};
			}
			const int32 as_ast_e23_right = int32(0);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return bool{};
			}
			return (as_ast_e23_left > as_ast_e23_right);
		}());
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
		if (as_ast_stmt_13_condition)
		{
			{
				TypedFrame.SetSourcePosition(25, 3);
				return ([&]() -> int32
			{
				const int32 as_ast_e29_left = ([&]() -> int32
			{
				const int32 as_ast_e27_arg0 = ([&]() -> int32
				{
					const int32 as_ast_e26_left = as_ast_d7;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e26_right = int32(1);
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e26_left, as_ast_e26_right);
				}());
				const int32 as_ast_e27_result = ASJIT_Helper_TypedASTMutualOdd_9b21b2a1cce2fd68_f3cd404bfc2f5d0c(Execution, as_ast_e27_arg0);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				return as_ast_e27_result;
			}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				const int32 as_ast_e29_right = int32(1);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e29_left, as_ast_e29_right);
			}());
			}
		}
	}
	{
		TypedFrame.SetSourcePosition(27, 2);
		return int32(100);
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int TypedASTMutualEven(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:21:1
// JIT Entry   : Raw
int32 ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_8588bac2591fc1969f2fc7fa17b3adab41afb88f6e971dbea48adf0af50f110b_Raw(FScriptExecution& Execution, asDWORD p_0)
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
	return ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_8588bac2591fc1969f2fc7fa17b3adab41afb88f6e971dbea48adf0af50f110b_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int TypedASTMutualEven(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:21:1
// JIT Entry   : VM
void ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_8588bac2591fc1969f2fc7fa17b3adab41afb88f6e971dbea48adf0af50f110b_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_8588bac2591fc1969f2fc7fa17b3adab41afb88f6e971dbea48adf0af50f110b_Raw(Execution,
		*(asDWORD*)(l_fp + 0));
}

// AS Function : int TypedASTMutualEven(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:21:1
// JIT Entry   : Parms
void ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_8588bac2591fc1969f2fc7fa17b3adab41afb88f6e971dbea48adf0af50f110b_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_8588bac2591fc1969f2fc7fa17b3adab41afb88f6e971dbea48adf0af50f110b_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
