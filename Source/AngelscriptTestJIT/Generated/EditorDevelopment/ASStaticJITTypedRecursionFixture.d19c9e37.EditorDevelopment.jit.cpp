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
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:50:5
 * StableFunctionKey  : 69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb
 * ExecutionHash      : a62ae34d1b052ab959b49dc87abd24714a779797ef722ba72f654d1fe56bef48
 * DebugHash          : e77f90e1d42ce084dcc3eec4d6de1a6259ff15b8d03cb5295f7aeb9b772305a5
 * EntryAbiHash       : 0bb8561a88e967bd0b6be6e0c1b70319bb70e04b1ecab786122674ec9f725c03
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a62ae34d1b052ab959b49dc87abd24714a779797ef722ba72f654d1fe56bef48_TypedBody(
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
		const int32 as_ast_e65_left = as_ast_d18;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		const int32 as_ast_e65_right = as_ast_d19;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e65_left, as_ast_e65_right);
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
static int32 ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a62ae34d1b052ab959b49dc87abd24714a779797ef722ba72f654d1fe56bef48_TypedBody(
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
			const int32 as_ast_e70_value = ([&]() -> int32
		{
			const int32 as_ast_e68_arg1 = as_ast_d22;
			const int32 as_ast_e68_arg0 = as_ast_d21;
			const int32 as_ast_e68_result = ASJIT_Helper_TypedASTExceptionHelper_b235edf227be8a1f_69188dd501ca6ad0(Execution, as_ast_e68_arg0, as_ast_e68_arg1);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			return as_ast_e68_result;
		}());
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			int32& as_ast_e70_target = as_ast_d23;
			as_ast_e70_target = as_ast_e70_value;
			return as_ast_e70_target;
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
			const int32 as_ast_e73_arg1 = int32(2);
			const int32 as_ast_e73_arg0 = int32(1);
			const int32 as_ast_e73_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>(Execution, ASJIT_Call_TypedASTProviderPrivateAdd_6b00a476_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb, as_ast_e73_arg0, as_ast_e73_arg1);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			return as_ast_e73_result;
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
		const int32 as_ast_e76_left = as_ast_d23;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		const int32 as_ast_e76_right = int32(99);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e76_left, as_ast_e76_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int TypedASTExceptionEntry(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:50:5
// JIT Entry   : Raw
int32 ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a62ae34d1b052ab959b49dc87abd24714a779797ef722ba72f654d1fe56bef48_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
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
	return ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a62ae34d1b052ab959b49dc87abd24714a779797ef722ba72f654d1fe56bef48_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int TypedASTExceptionEntry(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:50:5
// JIT Entry   : VM
void ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a62ae34d1b052ab959b49dc87abd24714a779797ef722ba72f654d1fe56bef48_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a62ae34d1b052ab959b49dc87abd24714a779797ef722ba72f654d1fe56bef48_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int TypedASTExceptionEntry(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:50:5
// JIT Entry   : Parms
void ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a62ae34d1b052ab959b49dc87abd24714a779797ef722ba72f654d1fe56bef48_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_69188dd501ca6ad04292b14ffce76fe7ee5a023a8dea4e9b1078a27e0ba5effb_a62ae34d1b052ab959b49dc87abd24714a779797ef722ba72f654d1fe56bef48_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTFrameEntry(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:36:5
 * StableFunctionKey  : 837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62
 * ExecutionHash      : 194e9bacc379bd33ab89799ed05f8d013f518748a72f50f33bec4d000d5b7a89
 * DebugHash          : 4990aa8cdae7f65be706d3da5cb0a6bfecad68c0fb4ed32df6797094ec6a7917
 * EntryAbiHash       : 9a370f34ced1e1b098f2f4b339f10de3f95ee0620d3cf38fd90049284735ef6e
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_194e9bacc379bd33ab89799ed05f8d013f518748a72f50f33bec4d000d5b7a89_TypedBody(
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
		const int32 as_ast_e45_left = ([&]() -> int32
	{
		const int32 as_ast_e43_arg0 = int32(2);
		const int32 as_ast_e43_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32>(Execution, ASJIT_Call_TypedASTObserveFrame_b9dfce83_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62, as_ast_e43_arg0);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_ast_e43_result;
	}());
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		const int32 as_ast_e45_right = as_ast_d9;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e45_left, as_ast_e45_right);
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
static int32 ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_194e9bacc379bd33ab89799ed05f8d013f518748a72f50f33bec4d000d5b7a89_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d13)
{
	int32 as_ast_d14;
	{
		TypedFrame.SetSourcePosition(38, 6);
		([&]() -> int32
		{
			const int32 as_ast_e49_value = ([&]() -> int32
		{
			const int32 as_ast_e47_arg0 = int32(1);
			const int32 as_ast_e47_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32>(Execution, ASJIT_Call_TypedASTObserveFrame_b9dfce83_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62, as_ast_e47_arg0);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			return as_ast_e47_result;
		}());
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			int32& as_ast_e49_target = as_ast_d14;
			as_ast_e49_target = as_ast_e49_value;
			return as_ast_e49_target;
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
			const int32 as_ast_e53_value = ([&]() -> int32
		{
			const int32 as_ast_e51_arg0 = as_ast_d13;
			const int32 as_ast_e51_result = ASJIT_Helper_TypedASTFrameHelper_abfbff5b1d349921_837d3cf600bbfd73(Execution, as_ast_e51_arg0);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			return as_ast_e51_result;
		}());
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			int32& as_ast_e53_target = as_ast_d15;
			as_ast_e53_target = as_ast_e53_value;
			return as_ast_e53_target;
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
			const int32 as_ast_e57_value = ([&]() -> int32
		{
			const int32 as_ast_e55_arg0 = int32(3);
			const int32 as_ast_e55_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32>(Execution, ASJIT_Call_TypedASTObserveFrame_b9dfce83_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62, as_ast_e55_arg0);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			return as_ast_e55_result;
		}());
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return int32{};
			}
			int32& as_ast_e57_target = as_ast_d16;
			as_ast_e57_target = as_ast_e57_value;
			return as_ast_e57_target;
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
		const int32 as_ast_e62_left = ([&]() -> int32
	{
		const int32 as_ast_e61_left = as_ast_d14;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		const int32 as_ast_e61_right = as_ast_d15;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e61_left, as_ast_e61_right);
	}());
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		const int32 as_ast_e62_right = as_ast_d16;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e62_left, as_ast_e62_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int TypedASTFrameEntry(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:36:5
// JIT Entry   : Raw
int32 ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_194e9bacc379bd33ab89799ed05f8d013f518748a72f50f33bec4d000d5b7a89_Raw(FScriptExecution& Execution, asDWORD p_0)
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
	return ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_194e9bacc379bd33ab89799ed05f8d013f518748a72f50f33bec4d000d5b7a89_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int TypedASTFrameEntry(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:36:5
// JIT Entry   : VM
void ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_194e9bacc379bd33ab89799ed05f8d013f518748a72f50f33bec4d000d5b7a89_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_194e9bacc379bd33ab89799ed05f8d013f518748a72f50f33bec4d000d5b7a89_Raw(Execution,
		*(asDWORD*)(l_fp + 0));
}

// AS Function : int TypedASTFrameEntry(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:36:5
// JIT Entry   : Parms
void ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_194e9bacc379bd33ab89799ed05f8d013f518748a72f50f33bec4d000d5b7a89_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_837d3cf600bbfd7302d972297b4f147f140db3587b726f99e61f72ce21c72d62_194e9bacc379bd33ab89799ed05f8d013f518748a72f50f33bec4d000d5b7a89_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTMutualOdd(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:11:5
 * StableFunctionKey  : 9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78
 * ExecutionHash      : 66fb55c2b89a6ade8eb3cbb5a35828045149d1f54345e74fc984288b2796c5e6
 * DebugHash          : 30a1ed13929ec27af96a6c07001341fff1e0d5c901dd30a7fbdf7d595bb7ff87
 * EntryAbiHash       : bf59be51292578d00c3cd680f28bc0c167217588050f08dd410162b09320888d
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_66fb55c2b89a6ade8eb3cbb5a35828045149d1f54345e74fc984288b2796c5e6 = "ASStaticJITTypedRecursionFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_66fb55c2b89a6ade8eb3cbb5a35828045149d1f54345e74fc984288b2796c5e6
#endif

// AS Function : int TypedASTMutualOdd(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:11:5
// JIT Entry   : Raw
int32 ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_66fb55c2b89a6ade8eb3cbb5a35828045149d1f54345e74fc984288b2796c5e6(FScriptExecution& Execution, asDWORD p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:11:5
// JIT Entry   : VM
void ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_66fb55c2b89a6ade8eb3cbb5a35828045149d1f54345e74fc984288b2796c5e6_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_66fb55c2b89a6ade8eb3cbb5a35828045149d1f54345e74fc984288b2796c5e6(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int TypedASTMutualOdd(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:11:5
// JIT Entry   : Parms
void ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_66fb55c2b89a6ade8eb3cbb5a35828045149d1f54345e74fc984288b2796c5e6_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_9b21b2a1cce2fd681ac9c627b4e5c9f95e5d810ef4c2438e7c9de235a94e5b78_66fb55c2b89a6ade8eb3cbb5a35828045149d1f54345e74fc984288b2796c5e6(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTFrameHelper(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:30:5
 * StableFunctionKey  : abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd
 * ExecutionHash      : b3eafab65b6ad861cd8d40c4ecfdf64f7fe4396870c93f6268c9be35ffc84fa4
 * DebugHash          : 20d08b6de4ea9067dce964a99e6266084a91eaa77bab8a59c909c1dd47409ee9
 * EntryAbiHash       : d445a389231e20b52c85114adb522d9422b08dc0a58b16efe7f59cb9513ee324
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_b3eafab65b6ad861cd8d40c4ecfdf64f7fe4396870c93f6268c9be35ffc84fa4 = "ASStaticJITTypedRecursionFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_b3eafab65b6ad861cd8d40c4ecfdf64f7fe4396870c93f6268c9be35ffc84fa4
#endif

// AS Function : int TypedASTFrameHelper(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:30:5
// JIT Entry   : Raw
int32 ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_b3eafab65b6ad861cd8d40c4ecfdf64f7fe4396870c93f6268c9be35ffc84fa4(FScriptExecution& Execution, asDWORD p_Value)
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
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:30:5
// JIT Entry   : VM
void ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_b3eafab65b6ad861cd8d40c4ecfdf64f7fe4396870c93f6268c9be35ffc84fa4_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_b3eafab65b6ad861cd8d40c4ecfdf64f7fe4396870c93f6268c9be35ffc84fa4(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : int TypedASTFrameHelper(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:30:5
// JIT Entry   : Parms
void ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_b3eafab65b6ad861cd8d40c4ecfdf64f7fe4396870c93f6268c9be35ffc84fa4_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_abfbff5b1d349921fd716663a12ea7b62451daf41490b03f25b11166037418dd_b3eafab65b6ad861cd8d40c4ecfdf64f7fe4396870c93f6268c9be35ffc84fa4(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTExceptionHelper(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:44:5
 * StableFunctionKey  : b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381
 * ExecutionHash      : fbba75d00a4262972855b120ee9f998fb1833203fddef05c42d1930cd6227625
 * DebugHash          : 9751a7ef706cd662bd710e297d541815ce7e09d60707722ea146e4148b44f7d9
 * EntryAbiHash       : 8a222551660204d8aa9ac9261fac2964b81809e1be0ba8355d19dccb11dd9971
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_fbba75d00a4262972855b120ee9f998fb1833203fddef05c42d1930cd6227625 = "ASStaticJITTypedRecursionFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_fbba75d00a4262972855b120ee9f998fb1833203fddef05c42d1930cd6227625
#endif

// AS Function : int TypedASTExceptionHelper(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:44:5
// JIT Entry   : Raw
int32 ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_fbba75d00a4262972855b120ee9f998fb1833203fddef05c42d1930cd6227625(FScriptExecution& Execution, asDWORD p_Numerator, asDWORD p_Denominator)
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
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:44:5
// JIT Entry   : VM
void ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_fbba75d00a4262972855b120ee9f998fb1833203fddef05c42d1930cd6227625_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_fbba75d00a4262972855b120ee9f998fb1833203fddef05c42d1930cd6227625(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}
// AS Function : int TypedASTExceptionHelper(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:44:5
// JIT Entry   : Parms
void ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_fbba75d00a4262972855b120ee9f998fb1833203fddef05c42d1930cd6227625_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Numerator = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1_Denominator = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_b235edf227be8a1fa8e69b94d443101d0fd4d4e751e28a4da8e14671f2d11381_fbba75d00a4262972855b120ee9f998fb1833203fddef05c42d1930cd6227625(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Numerator),
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_1_Denominator));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTSelfRecursion(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:2:5
 * StableFunctionKey  : efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733
 * ExecutionHash      : b25adb6353b061dccbf6dcf5f6b10e3cbc339912da5beef0a25ad497b36d4c2f
 * DebugHash          : 36f9355aed0db379bea998f41321ccd582fa98b451b05750cf25669cc1c23d2d
 * EntryAbiHash       : 6d11fd1ead83143bf8f550656196082956507d4c82dc84aeb65e3e9c5bde4cb5
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_b25adb6353b061dccbf6dcf5f6b10e3cbc339912da5beef0a25ad497b36d4c2f_TypedBody(
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
	return ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_b25adb6353b061dccbf6dcf5f6b10e3cbc339912da5beef0a25ad497b36d4c2f_TypedBody(
		Execution, TypedFrame, as_helper_p0);
}

static int32 ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_b25adb6353b061dccbf6dcf5f6b10e3cbc339912da5beef0a25ad497b36d4c2f_TypedBody(
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
				const int32 as_ast_e10_left = as_ast_d3;
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				const int32 as_ast_e10_right = ([&]() -> int32
			{
				const int32 as_ast_e9_arg0 = ([&]() -> int32
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
				const int32 as_ast_e9_result = ASJIT_Helper_TypedASTSelfRecursion_efe75e2004503ee7_efe75e2004503ee7(Execution, as_ast_e9_arg0);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				return as_ast_e9_result;
			}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e10_left, as_ast_e10_right);
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
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:2:5
// JIT Entry   : Raw
int32 ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_b25adb6353b061dccbf6dcf5f6b10e3cbc339912da5beef0a25ad497b36d4c2f_Raw(FScriptExecution& Execution, asDWORD p_0)
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
	return ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_b25adb6353b061dccbf6dcf5f6b10e3cbc339912da5beef0a25ad497b36d4c2f_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int TypedASTSelfRecursion(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:2:5
// JIT Entry   : VM
void ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_b25adb6353b061dccbf6dcf5f6b10e3cbc339912da5beef0a25ad497b36d4c2f_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_b25adb6353b061dccbf6dcf5f6b10e3cbc339912da5beef0a25ad497b36d4c2f_Raw(Execution,
		*(asDWORD*)(l_fp + 0));
}

// AS Function : int TypedASTSelfRecursion(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:2:5
// JIT Entry   : Parms
void ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_b25adb6353b061dccbf6dcf5f6b10e3cbc339912da5beef0a25ad497b36d4c2f_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_efe75e2004503ee73f7d821394e777a4f886f4fed6fbc653757ce63fbaf27733_b25adb6353b061dccbf6dcf5f6b10e3cbc339912da5beef0a25ad497b36d4c2f_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTMutualEven(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:21:5
 * StableFunctionKey  : f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce
 * ExecutionHash      : a5b5e409bb3d70b89b7f5c03d49e3302bda5f71a77bf2f4c2fd9344d2a0b0f71
 * DebugHash          : 279a1b1b104ef03fa443ec247ae4a249e1b0ce476fbf65cdc23a6ba5a1b4c668
 * EntryAbiHash       : fa5cbe60f891ca77e110682e6c1613af4186ed9e47cbc9d0531d8dde05cd3a5c
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_HelperBody_TypedASTMutualOdd_9b21b2a1cce2fd68_f3cd404bfc2f5d0c(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d5);
static int32 ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_a5b5e409bb3d70b89b7f5c03d49e3302bda5f71a77bf2f4c2fd9344d2a0b0f71_TypedBody(
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
			const int32 as_ast_e16_left = as_ast_d5;
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return bool{};
			}
			const int32 as_ast_e16_right = int32(0);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return bool{};
			}
			return (as_ast_e16_left > as_ast_e16_right);
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
				const int32 as_ast_e23_left = ([&]() -> int32
			{
				const int32 as_ast_e21_arg0 = ([&]() -> int32
				{
					const int32 as_ast_e19_left = as_ast_d5;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e19_right = int32(1);
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e19_left, as_ast_e19_right);
				}());
				const int32 as_ast_e21_result = ASJIT_Helper_TypedASTMutualEven_f3cd404bfc2f5d0c_f3cd404bfc2f5d0c(Execution, as_ast_e21_arg0);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				return as_ast_e21_result;
			}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				const int32 as_ast_e23_right = int32(1);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e23_left, as_ast_e23_right);
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
	return ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_a5b5e409bb3d70b89b7f5c03d49e3302bda5f71a77bf2f4c2fd9344d2a0b0f71_TypedBody(
		Execution, TypedFrame, as_helper_p0);
}

static int32 ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_a5b5e409bb3d70b89b7f5c03d49e3302bda5f71a77bf2f4c2fd9344d2a0b0f71_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d7)
{
	{
		TypedFrame.SetSourcePosition(23, 2);
		const bool as_ast_stmt_13_condition = ([&]() -> bool
		{
			const int32 as_ast_e30_left = as_ast_d7;
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return bool{};
			}
			const int32 as_ast_e30_right = int32(0);
			if (UNLIKELY(Execution.bExceptionThrown))
			{
				return bool{};
			}
			return (as_ast_e30_left > as_ast_e30_right);
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
				const int32 as_ast_e37_left = ([&]() -> int32
			{
				const int32 as_ast_e35_arg0 = ([&]() -> int32
				{
					const int32 as_ast_e33_left = as_ast_d7;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e33_right = int32(1);
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e33_left, as_ast_e33_right);
				}());
				const int32 as_ast_e35_result = ASJIT_Helper_TypedASTMutualOdd_9b21b2a1cce2fd68_f3cd404bfc2f5d0c(Execution, as_ast_e35_arg0);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				return as_ast_e35_result;
			}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				const int32 as_ast_e37_right = int32(1);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e37_left, as_ast_e37_right);
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
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:21:5
// JIT Entry   : Raw
int32 ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_a5b5e409bb3d70b89b7f5c03d49e3302bda5f71a77bf2f4c2fd9344d2a0b0f71_Raw(FScriptExecution& Execution, asDWORD p_0)
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
	return ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_a5b5e409bb3d70b89b7f5c03d49e3302bda5f71a77bf2f4c2fd9344d2a0b0f71_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int TypedASTMutualEven(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:21:5
// JIT Entry   : VM
void ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_a5b5e409bb3d70b89b7f5c03d49e3302bda5f71a77bf2f4c2fd9344d2a0b0f71_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_a5b5e409bb3d70b89b7f5c03d49e3302bda5f71a77bf2f4c2fd9344d2a0b0f71_Raw(Execution,
		*(asDWORD*)(l_fp + 0));
}

// AS Function : int TypedASTMutualEven(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedRecursionFixture.as:21:5
// JIT Entry   : Parms
void ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_a5b5e409bb3d70b89b7f5c03d49e3302bda5f71a77bf2f4c2fd9344d2a0b0f71_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_f3cd404bfc2f5d0cb21bd27c84491cb3feaeaa4979c279113354ebbfe88eebce_a5b5e409bb3d70b89b7f5c03d49e3302bda5f71a77bf2f4c2fd9344d2a0b0f71_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
