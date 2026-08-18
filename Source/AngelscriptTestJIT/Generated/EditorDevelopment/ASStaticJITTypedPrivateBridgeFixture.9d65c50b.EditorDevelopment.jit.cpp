// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as
 * CanonicalModuleName : ASStaticJITTypedPrivateBridgeFixture
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : 9d65c50b9c49e0b94a37b8336c334b41aafa52f6ae48344103e7a94fbd32231a
 * ProviderId          : 7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44
 * ArtifactProfile     : b58efc4c9f40879f0b98bcb54704d1b342ae56948a00e9ddbaa709787226e0c8
 * NativeEnvironment   : 76d620ec7d0db38aa2028996e595f75523f201e7e560c05d8c9000d60d04cd73
 * FunctionCount       : 5
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
 * Declaration       : int TypedASTRuntimeThunkShowcase(const FString&in)
 * Source             : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:8:1
 * StableFunctionKey  : 59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e
 * ExecutionHash      : 903fda0724edd107a42dab536e0562a329f5aa488514a2cf71fc6942fb4a9cb6
 * DebugHash          : 84c4c455833d298a19e72e37e3dd5fd456053781b60624be02a142f198bb4733
 * EntryAbiHash       : eafaed37b176f7dc1025f3c86200feb10fb066ef6f083b8ce31d94120dc42f21
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e_903fda0724edd107a42dab536e0562a329f5aa488514a2cf71fc6942fb4a9cb6_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const FString& as_sem_s0);

/*
 * AngelScript Native Call Site 0
 * AS Declaration      : int TypedASTRuntimeThunkProbe(const FString&in)
 * Emitted C++ Callee  : AngelscriptStaticJITNativeCallables::RuntimeThunkProbe
 * Route               : RuntimeThunk
 * Effective Target    : AngelscriptStaticJITNativeCallables::RuntimeThunkProbe
 * Runtime DLL Core    : <none>
 * Metadata Row        : <none>
 * Reference Slot      : -1
 */
static int32 ASJIT_59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e_903fda0724edd107a42dab536e0562a329f5aa488514a2cf71fc6942fb4a9cb6_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const FString& as_sem_s0)
{
	TypedFrame.SetSourcePosition(10, 2);
	return ([&]() -> int32
	{
		TypedFrame.SetSourcePosition(10, 9);
		const FString& as_sem_e2_arg0 = as_sem_s0;
		return AngelscriptStaticJITNativeCallables::RuntimeThunkProbe(as_sem_e2_arg0);
	}());
}

// AS Function : int TypedASTRuntimeThunkShowcase(const FString&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:8:1
// JIT Entry   : Raw
int32 ASJIT_59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e_903fda0724edd107a42dab536e0562a329f5aa488514a2cf71fc6942fb4a9cb6_Raw(FScriptExecution& Execution, FString* p_0)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as",
		"int TypedASTRuntimeThunkShowcase(const FString&in)",
		8,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e_903fda0724edd107a42dab536e0562a329f5aa488514a2cf71fc6942fb4a9cb6_TypedBody(TypedFrame, *p_0);
}

// AS Function : int TypedASTRuntimeThunkShowcase(const FString&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:8:1
// JIT Entry   : VM
void ASJIT_59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e_903fda0724edd107a42dab536e0562a329f5aa488514a2cf71fc6942fb4a9cb6_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e_903fda0724edd107a42dab536e0562a329f5aa488514a2cf71fc6942fb4a9cb6_Raw(Execution,
		*(FString**)(l_fp + 0));
}

// AS Function : int TypedASTRuntimeThunkShowcase(const FString&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:8:1
// JIT Entry   : Parms
void ASJIT_59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e_903fda0724edd107a42dab536e0562a329f5aa488514a2cf71fc6942fb4a9cb6_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(FString);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_59acf1e8407b0391645aa3d2d8820c635615ea548e9b9326e488a5d12bebf48e_903fda0724edd107a42dab536e0562a329f5aa488514a2cf71fc6942fb4a9cb6_Raw(Execution,
		(FString*)(((SIZE_T)Parms) + ParmOffset_0));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTPrivateBridgeShowcase(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:2:1
 * StableFunctionKey  : 6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e
 * ExecutionHash      : 41fe6656ff1dcc4b64399ccd16d4fb070a4c5f98f8028039779d2f452a183753
 * DebugHash          : 85a5bdbe8d14c58398fc7a11eb45675f1e7584d0d5677b4c8c09f8ed282af10d
 * EntryAbiHash       : 32c0c27f47fe1029322c0ce7e17536b36a031cc3ce9c01a4ee37be56f1b14a15
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e_41fe6656ff1dcc4b64399ccd16d4fb070a4c5f98f8028039779d2f452a183753_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1);

static const FAngelscriptTypedASTJITBoundCallSite ASJIT_Call_TypedASTProviderPrivateAdd_6b00a476_6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e = {
	TEXT("int TypedASTProviderPrivateAdd(int, int)"),
	TEXT("AngelscriptStaticJITAotFixture::TypedASTProviderPrivateAdd"),
	FAngelscriptStableFunctionKey{FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("6b00a476bd57a82fb216373cb0eef27d00c4acb64af32b342c47c9b7dee91aac"))) }},
	FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("304fbe10a2078ab14e2f67d70c35fd3322a0e9ce8b25e1915cebe00e271b6e96"))) },
	0
};

/*
 * AngelScript Native Call Site 0
 * AS Declaration      : int TypedASTProviderPrivateAdd(int, int)
 * Emitted C++ Callee  : AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>
 * Route               : CurrentNativeBinding
 * Effective Target    : AngelscriptStaticJITAotFixture::TypedASTProviderPrivateAdd
 * Runtime DLL Core    : AngelscriptTypedASTJIT::ResolveBoundNative
 * Metadata Row        : ASJIT_Call_TypedASTProviderPrivateAdd_6b00a476_6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e
 * Reference Slot      : 0
 */
static int32 ASJIT_6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e_41fe6656ff1dcc4b64399ccd16d4fb070a4c5f98f8028039779d2f452a183753_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1)
{
	TypedFrame.SetSourcePosition(4, 2);
	return ([&]() -> int32
	{
		TypedFrame.SetSourcePosition(4, 9);
		const int32 as_sem_e2_arg1 = as_sem_s1;
		const int32 as_sem_e2_arg0 = as_sem_s0;
		const int32 as_sem_e2_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>(
			Execution, ASJIT_Call_TypedASTProviderPrivateAdd_6b00a476_6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e, as_sem_e2_arg0, as_sem_e2_arg1);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_sem_e2_result;
	}());
}

// AS Function : int TypedASTPrivateBridgeShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:2:1
// JIT Entry   : Raw
int32 ASJIT_6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e_41fe6656ff1dcc4b64399ccd16d4fb070a4c5f98f8028039779d2f452a183753_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as",
		"int TypedASTPrivateBridgeShowcase(const int, const int)",
		2,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e_41fe6656ff1dcc4b64399ccd16d4fb070a4c5f98f8028039779d2f452a183753_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int TypedASTPrivateBridgeShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:2:1
// JIT Entry   : VM
void ASJIT_6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e_41fe6656ff1dcc4b64399ccd16d4fb070a4c5f98f8028039779d2f452a183753_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e_41fe6656ff1dcc4b64399ccd16d4fb070a4c5f98f8028039779d2f452a183753_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int TypedASTPrivateBridgeShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:2:1
// JIT Entry   : Parms
void ASJIT_6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e_41fe6656ff1dcc4b64399ccd16d4fb070a4c5f98f8028039779d2f452a183753_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e_41fe6656ff1dcc4b64399ccd16d4fb070a4c5f98f8028039779d2f452a183753_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : float32 TypedASTOrdinaryBridgeSqrt(const float32)
 * Source             : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:28:1
 * StableFunctionKey  : 6bd24036f49f790093d22b8049bf8809569e0807a54b7e105d64d1385454542e
 * ExecutionHash      : 2324792fb0f2db6ab8bc9f893e7b3cbe5cae4fa30287c5dd799abdd5a15968c4
 * DebugHash          : ba85cc8259d645508ea9683c087ee1a3dd471f271c0d87a1171e64c4cacc8f4e
 * EntryAbiHash       : 952945aab19a52aa87e0363f0c98f326892eee81f714fdb4ffad4f6e2f460a75
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static float ASJIT_6bd24036f49f790093d22b8049bf8809569e0807a54b7e105d64d1385454542e_2324792fb0f2db6ab8bc9f893e7b3cbe5cae4fa30287c5dd799abdd5a15968c4_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const float as_sem_s0);

static const FAngelscriptTypedASTJITBoundCallSite ASJIT_Call_Sqrt_000f5a28_6bd24036f49f790093d22b8049bf8809569e0807a54b7e105d64d1385454542e = {
	TEXT("float32 Sqrt(float32)"),
	TEXT("FMath::Sqrt"),
	FAngelscriptStableFunctionKey{FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("000f5a286aed98174036c0f28f63aacbeaa6419aeacc1f82f95f1f5761d67f5f"))) }},
	FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("4b41de3d4cf1aaf3ad57a76332392fba66c9e3f18dcf7f398c837f6819fbb107"))) },
	0
};

/*
 * AngelScript Native Call Site 0
 * AS Declaration      : float32 Sqrt(float32)
 * Emitted C++ Callee  : AngelscriptTypedASTJIT::InvokeBound<float, float>
 * Route               : Bridge
 * Effective Target    : FMath::Sqrt
 * Runtime DLL Core    : AngelscriptTypedASTJIT::InvokeBoundViaVM
 * Metadata Row        : ASJIT_Call_Sqrt_000f5a28_6bd24036f49f790093d22b8049bf8809569e0807a54b7e105d64d1385454542e
 * Reference Slot      : 0
 */
static float ASJIT_6bd24036f49f790093d22b8049bf8809569e0807a54b7e105d64d1385454542e_2324792fb0f2db6ab8bc9f893e7b3cbe5cae4fa30287c5dd799abdd5a15968c4_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const float as_sem_s0)
{
	TypedFrame.SetSourcePosition(30, 2);
	return ([&]() -> float
	{
		TypedFrame.SetSourcePosition(30, 9);
		const float as_sem_e1_arg0 = as_sem_s0;
		const float as_sem_e1_result = AngelscriptTypedASTJIT::InvokeBound<float, float>(
			Execution, ASJIT_Call_Sqrt_000f5a28_6bd24036f49f790093d22b8049bf8809569e0807a54b7e105d64d1385454542e, as_sem_e1_arg0);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return float{};
		}
		return as_sem_e1_result;
	}());
}

// AS Function : float32 TypedASTOrdinaryBridgeSqrt(const float32)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:28:1
// JIT Entry   : Raw
float ASJIT_6bd24036f49f790093d22b8049bf8809569e0807a54b7e105d64d1385454542e_2324792fb0f2db6ab8bc9f893e7b3cbe5cae4fa30287c5dd799abdd5a15968c4_Raw(FScriptExecution& Execution, float p_0)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as",
		"float32 TypedASTOrdinaryBridgeSqrt(const float32)",
		28,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_6bd24036f49f790093d22b8049bf8809569e0807a54b7e105d64d1385454542e_2324792fb0f2db6ab8bc9f893e7b3cbe5cae4fa30287c5dd799abdd5a15968c4_TypedBody(Execution, TypedFrame, static_cast<float>(p_0));
}

// AS Function : float32 TypedASTOrdinaryBridgeSqrt(const float32)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:28:1
// JIT Entry   : VM
void ASJIT_6bd24036f49f790093d22b8049bf8809569e0807a54b7e105d64d1385454542e_2324792fb0f2db6ab8bc9f893e7b3cbe5cae4fa30287c5dd799abdd5a15968c4_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(float*)l_outValue = ASJIT_6bd24036f49f790093d22b8049bf8809569e0807a54b7e105d64d1385454542e_2324792fb0f2db6ab8bc9f893e7b3cbe5cae4fa30287c5dd799abdd5a15968c4_Raw(Execution,
		*(float*)(l_fp + 0));
}

// AS Function : float32 TypedASTOrdinaryBridgeSqrt(const float32)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:28:1
// JIT Entry   : Parms
void ASJIT_6bd24036f49f790093d22b8049bf8809569e0807a54b7e105d64d1385454542e_2324792fb0f2db6ab8bc9f893e7b3cbe5cae4fa30287c5dd799abdd5a15968c4_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(float);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(float));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(float*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_6bd24036f49f790093d22b8049bf8809569e0807a54b7e105d64d1385454542e_2324792fb0f2db6ab8bc9f893e7b3cbe5cae4fa30287c5dd799abdd5a15968c4_Raw(Execution,
		static_cast<float>(*(float*)(((SIZE_T)Parms) + ParmOffset_0)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int TypedASTPrivateBridgeException(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:14:1
 * StableFunctionKey  : b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151
 * ExecutionHash      : 8f65bcc7986569e5633cc61f4f45fdcaddd25f3663a8e59a6019f7c76abe92ea
 * DebugHash          : 9165c8c4d9384458363bc3ebef2e2c24eb5634e60ad65856a6a7081c2ae7e87e
 * EntryAbiHash       : 76649d45394c9fe464a49e2719a1afabbb0fdc9e8b0de6622e622d4ebfa0d800
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151_8f65bcc7986569e5633cc61f4f45fdcaddd25f3663a8e59a6019f7c76abe92ea_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0);

static const FAngelscriptTypedASTJITBoundCallSite ASJIT_Call_TypedASTProviderPrivateFail_fc7ed8ea_b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151 = {
	TEXT("int TypedASTProviderPrivateFail(int)"),
	TEXT("AngelscriptStaticJITAotFixture::TypedASTProviderPrivateFail"),
	FAngelscriptStableFunctionKey{FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("fc7ed8ea4c660ec30658e3fa4626a8a93add6e870b7f100e3b08cace133511b4"))) }},
	FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("e3326c3f43f4aa53f1d1840bf07e12368bbd0ed6e1ecf53819152d6fc2914a47"))) },
	1
};

static const FAngelscriptTypedASTJITBoundCallSite ASJIT_Call_TypedASTProviderPrivateAdd_6b00a476_b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151 = {
	TEXT("int TypedASTProviderPrivateAdd(int, int)"),
	TEXT("AngelscriptStaticJITAotFixture::TypedASTProviderPrivateAdd"),
	FAngelscriptStableFunctionKey{FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("6b00a476bd57a82fb216373cb0eef27d00c4acb64af32b342c47c9b7dee91aac"))) }},
	FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("304fbe10a2078ab14e2f67d70c35fd3322a0e9ce8b25e1915cebe00e271b6e96"))) },
	0
};

/*
 * AngelScript Native Call Site 0
 * AS Declaration      : int TypedASTProviderPrivateFail(int)
 * Emitted C++ Callee  : AngelscriptTypedASTJIT::InvokeBound<int32, int32>
 * Route               : Bridge
 * Effective Target    : AngelscriptStaticJITAotFixture::TypedASTProviderPrivateFail
 * Runtime DLL Core    : AngelscriptTypedASTJIT::InvokeBoundViaVM
 * Metadata Row        : ASJIT_Call_TypedASTProviderPrivateFail_fc7ed8ea_b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151
 * Reference Slot      : 1
 */
/*
 * AngelScript Native Call Site 1
 * AS Declaration      : int TypedASTProviderPrivateAdd(int, int)
 * Emitted C++ Callee  : AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>
 * Route               : CurrentNativeBinding
 * Effective Target    : AngelscriptStaticJITAotFixture::TypedASTProviderPrivateAdd
 * Runtime DLL Core    : AngelscriptTypedASTJIT::ResolveBoundNative
 * Metadata Row        : ASJIT_Call_TypedASTProviderPrivateAdd_6b00a476_b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151
 * Reference Slot      : 0
 */
static int32 ASJIT_b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151_8f65bcc7986569e5633cc61f4f45fdcaddd25f3663a8e59a6019f7c76abe92ea_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0)
{
	TypedFrame.SetSourcePosition(16, 6);
	const int32 as_sem_s1 = ([&]() -> int32
	{
		TypedFrame.SetSourcePosition(16, 15);
		const int32 as_sem_e1_arg0 = as_sem_s0;
		const int32 as_sem_e1_result = AngelscriptTypedASTJIT::InvokeBound<int32, int32>(
			Execution, ASJIT_Call_TypedASTProviderPrivateFail_fc7ed8ea_b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151, as_sem_e1_arg0);
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
	TypedFrame.SetSourcePosition(17, 2);
	([&]() -> int32
	{
		TypedFrame.SetSourcePosition(17, 2);
		const int32 as_sem_e4_arg1 = int32(2);
		const int32 as_sem_e4_arg0 = int32(1);
		const int32 as_sem_e4_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>(
			Execution, ASJIT_Call_TypedASTProviderPrivateAdd_6b00a476_b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151, as_sem_e4_arg0, as_sem_e4_arg1);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_sem_e4_result;
	}());
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	TypedFrame.SetSourcePosition(18, 2);
	return as_sem_s1;
}

// AS Function : int TypedASTPrivateBridgeException(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:14:1
// JIT Entry   : Raw
int32 ASJIT_b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151_8f65bcc7986569e5633cc61f4f45fdcaddd25f3663a8e59a6019f7c76abe92ea_Raw(FScriptExecution& Execution, asDWORD p_0)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as",
		"int TypedASTPrivateBridgeException(const int)",
		14,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151_8f65bcc7986569e5633cc61f4f45fdcaddd25f3663a8e59a6019f7c76abe92ea_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int TypedASTPrivateBridgeException(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:14:1
// JIT Entry   : VM
void ASJIT_b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151_8f65bcc7986569e5633cc61f4f45fdcaddd25f3663a8e59a6019f7c76abe92ea_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151_8f65bcc7986569e5633cc61f4f45fdcaddd25f3663a8e59a6019f7c76abe92ea_Raw(Execution,
		*(asDWORD*)(l_fp + 0));
}

// AS Function : int TypedASTPrivateBridgeException(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:14:1
// JIT Entry   : Parms
void ASJIT_b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151_8f65bcc7986569e5633cc61f4f45fdcaddd25f3663a8e59a6019f7c76abe92ea_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_b704113d245323c0121962633d2387619eca73a2b1a5ca06d5f221de36c0b151_8f65bcc7986569e5633cc61f4f45fdcaddd25f3663a8e59a6019f7c76abe92ea_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : void TypedASTPublicUASFunctionExceptionEntry(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:21:1
 * StableFunctionKey  : d5b7a0ab04246da202d293f48aaa204d9b9e136c80fa24feb1a1b838502618af
 * ExecutionHash      : 837ba82648f990929e747be3d031a425fbe2d3f28120e8113cef2b6922245046
 * DebugHash          : 5ea377172c317af7d0b619bf3785990a6f6164fea778126773508e021c9fa31d
 * EntryAbiHash       : 8e7cfb4c503bbc4c146b962a4462542c886d3df8bba3eb584a9ef3ccce444fc7
 * ExecutionCaps      : 0
 * RuntimeTriggers    : 0
 * CapabilityHash     : 0000000000000000000000000000000000000000000000000000000000000000
 */
#if AS_JIT_DEBUG_CALLSTACKS
#undef SCRIPT_DEBUG_FILENAME
static const char* MODULENAME_ASJIT_d5b7a0ab04246da202d293f48aaa204d9b9e136c80fa24feb1a1b838502618af_837ba82648f990929e747be3d031a425fbe2d3f28120e8113cef2b6922245046 = "ASStaticJITTypedPrivateBridgeFixture";
#define SCRIPT_DEBUG_FILENAME MODULENAME_ASJIT_d5b7a0ab04246da202d293f48aaa204d9b9e136c80fa24feb1a1b838502618af_837ba82648f990929e747be3d031a425fbe2d3f28120e8113cef2b6922245046
#endif

// AS Function : void TypedASTPublicUASFunctionExceptionEntry(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:21:1
// JIT Entry   : Raw
void ASJIT_d5b7a0ab04246da202d293f48aaa204d9b9e136c80fa24feb1a1b838502618af_837ba82648f990929e747be3d031a425fbe2d3f28120e8113cef2b6922245046(FScriptExecution& Execution, asDWORD p_Value)
{
// == Jit at BC 0 ==
SCRIPT_DEBUG_CALLSTACK_FRAME("void TypedASTPublicUASFunctionExceptionEntry(int)", 23);
SCRIPT_ASSUME_NO_EXCEPTION()
alignas(8) asBYTE l_stack[8];
asQWORD l_valueRegister = 0;
asBYTE l_byteRegister = 0;
asDWORD l_dwordRegister = 0;
float l_floatRegister = 0;
double l_doubleRegister = 0;
void* l_objectRegister = nullptr;
asDWORD v_TEMP_dword_1 = {};
asDWORD v_TEMP_dword_2 = {};
asDWORD v_TEMP_dword_3 = {};
// JitEntry *
// SUSPEND
// JitEntry *
// PshV4 v0
// CALLSYS *
// void TypedASTInvokePublicUASFunctionException(int Value)
// Materialize whole stack
value_assign_safe<asDWORD>(&l_stack[4], p_Value);
{
SCRIPT_DEBUG_CALLSTACK_POSITION(23, 2);
  asCScriptFunction* ScriptFunc = FAngelscriptJITGeneratedReferenceAccess::GetScriptFunction(Execution, 1u);
SCRIPT_CALL_NATIVE(ScriptFunc, 4);
if (Execution.bExceptionThrown)
{
return;
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// PshC4 4
// PshC4 3
// CALLSYS *
// int TypedASTProviderPrivateAdd(int Left, int Right)
{
  asFUNCTION_t RawFuncPtr = FAngelscriptJITGeneratedReferenceAccess::GetSystemFunctionPointer(Execution, 0u).GetFunction();
  auto CastedFuncPtr = (int32(*)(int32,int32))RawFuncPtr;
SCRIPT_DEBUG_CALLSTACK_POSITION(24, 2);
int32 FunctionReturnValue = CastedFuncPtr(value_as<int32>(((asDWORD)0x3u)),value_as<int32>(((asDWORD)0x4u)));
l_dwordRegister = (asDWORD&)FunctionReturnValue;
if (Execution.bExceptionThrown) [[unlikely]]
{
return;
}
}
// JitEntry *
// SUSPEND
// JitEntry *
// RET 1
  return;
}
// AS Function : void TypedASTPublicUASFunctionExceptionEntry(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:21:1
// JIT Entry   : VM
void ASJIT_d5b7a0ab04246da202d293f48aaa204d9b9e136c80fa24feb1a1b838502618af_837ba82648f990929e747be3d031a425fbe2d3f28120e8113cef2b6922245046_VMEntry(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	ASJIT_d5b7a0ab04246da202d293f48aaa204d9b9e136c80fa24feb1a1b838502618af_837ba82648f990929e747be3d031a425fbe2d3f28120e8113cef2b6922245046(Execution,
		*(asDWORD*)(l_fp + 0));
}
// AS Function : void TypedASTPublicUASFunctionExceptionEntry(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:21:1
// JIT Entry   : Parms
void ASJIT_d5b7a0ab04246da202d293f48aaa204d9b9e136c80fa24feb1a1b838502618af_837ba82648f990929e747be3d031a425fbe2d3f28120e8113cef2b6922245046_ParmsEntry(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0_Value = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ASJIT_d5b7a0ab04246da202d293f48aaa204d9b9e136c80fa24feb1a1b838502618af_837ba82648f990929e747be3d031a425fbe2d3f28120e8113cef2b6922245046(Execution,
		(asDWORD)*(int32*)(((SIZE_T)Parms) + ParmOffset_0_Value));
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
