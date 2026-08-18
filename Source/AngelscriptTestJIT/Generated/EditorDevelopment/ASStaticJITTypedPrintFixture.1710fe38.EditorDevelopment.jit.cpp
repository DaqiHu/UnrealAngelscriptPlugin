// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITTypedPrintFixture.as
 * CanonicalModuleName : ASStaticJITTypedPrintFixture
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : 1710fe388041380445cd2bdea17ac27dff4806c8272518ee43733cd4175ca1cc
 * ProviderId          : 7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44
 * ArtifactProfile     : b58efc4c9f40879f0b98bcb54704d1b342ae56948a00e9ddbaa709787226e0c8
 * NativeEnvironment   : 76d620ec7d0db38aa2028996e595f75523f201e7e560c05d8c9000d60d04cd73
 * FunctionCount       : 1
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
 * Declaration       : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
 * Source             : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:1
 * StableFunctionKey  : 9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4
 * ExecutionHash      : 3d287fec96570ae323b5be5f87799c96aff6472b47dfb88cd4de99ab40b03762
 * DebugHash          : 636443155f3600493c3c1febfd095150c987de2da9db64ac7a9fb82658c7520b
 * EntryAbiHash       : 97fbcd10bc26610e92f0705696227e7d6c0a85c0ece2feeca0b53d76f314e25a
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_3d287fec96570ae323b5be5f87799c96aff6472b47dfb88cd4de99ab40b03762_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const FString& as_sem_s0,
	const double as_sem_s1,
	const FLinearColor& as_sem_s2);

/*
 * AngelScript Native Call Site 0
 * AS Declaration      : void Print(const FString&inout, float32 = 5.f, FLinearColor = FLinearColor :: LucBlue)
 * Emitted C++ Callee  : AngelscriptStaticJITNativeCallables::Print
 * Route               : DirectExported
 * Effective Target    : AngelscriptStaticJITNativeCallables::Print
 * Runtime DLL Core    : <none>
 * Metadata Row        : <none>
 * Reference Slot      : -1
 */
static int32 ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_3d287fec96570ae323b5be5f87799c96aff6472b47dfb88cd4de99ab40b03762_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const FString& as_sem_s0,
	const double as_sem_s1,
	const FLinearColor& as_sem_s2)
{
	TypedFrame.SetSourcePosition(7, 2);
	([&]() -> void
	{
		TypedFrame.SetSourcePosition(7, 2);
		const FLinearColor& as_sem_e6_arg2 = as_sem_s2;
		const float as_sem_e6_arg1 = static_cast<float>(as_sem_s1);
		const FString& as_sem_e6_arg0 = as_sem_s0;
		return AngelscriptStaticJITNativeCallables::Print(as_sem_e6_arg0, as_sem_e6_arg1, as_sem_e6_arg2);
	}());
	TypedFrame.SetSourcePosition(8, 2);
	return int32(314);
}

// AS Function : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:1
// JIT Entry   : Raw
int32 ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_3d287fec96570ae323b5be5f87799c96aff6472b47dfb88cd4de99ab40b03762_Raw(FScriptExecution& Execution, FString* p_0, double p_1, FLinearColor* p_2)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedPrintFixture.as",
		"int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)",
		2,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_3d287fec96570ae323b5be5f87799c96aff6472b47dfb88cd4de99ab40b03762_TypedBody(TypedFrame, *p_0, static_cast<double>(p_1), *p_2);
}

// AS Function : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:1
// JIT Entry   : VM
void ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_3d287fec96570ae323b5be5f87799c96aff6472b47dfb88cd4de99ab40b03762_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_3d287fec96570ae323b5be5f87799c96aff6472b47dfb88cd4de99ab40b03762_Raw(Execution,
		*(FString**)(l_fp + 0),
		*(double*)(l_fp + 2),
		*(FLinearColor**)(l_fp + 4));
}

// AS Function : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:1
// JIT Entry   : Parms
void ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_3d287fec96570ae323b5be5f87799c96aff6472b47dfb88cd4de99ab40b03762_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(FString);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(FLinearColor));
	const SIZE_T ParmOffset_2 = ParmsOffset;
	ParmsOffset += sizeof(FLinearColor);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_3d287fec96570ae323b5be5f87799c96aff6472b47dfb88cd4de99ab40b03762_Raw(Execution,
		(FString*)(((SIZE_T)Parms) + ParmOffset_0),
		static_cast<double>(*(double*)(((SIZE_T)Parms) + ParmOffset_1)),
		(FLinearColor*)(((SIZE_T)Parms) + ParmOffset_2));
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
