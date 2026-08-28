// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITTypedPrintFixture.as
 * CanonicalModuleName : ASStaticJITTypedPrintFixture
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : 1710fe388041380445cd2bdea17ac27dff4806c8272518ee43733cd4175ca1cc
 * ProviderId          : 7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44
 * ArtifactProfile     : 018cc71ab6612eca7608aa13abe1f7a1a7d81964bb8cc5821f1b1e047fcf7a11
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
 * Source             : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:5
 * StableFunctionKey  : 9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4
 * ExecutionHash      : 22520f4a76bcce5aecea9e6cf76f4b4e677dd04139ba6f5432f5fc11fe35b0ae
 * DebugHash          : 36f43385af144221f040238a169f370914ca7474de6082afa6911c20a2943f43
 * EntryAbiHash       : 97fbcd10bc26610e92f0705696227e7d6c0a85c0ece2feeca0b53d76f314e25a
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_22520f4a76bcce5aecea9e6cf76f4b4e677dd04139ba6f5432f5fc11fe35b0ae_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const FString& as_ast_d3,
	const double as_ast_d4,
	const FLinearColor& as_ast_d5);

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
static int32 ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_22520f4a76bcce5aecea9e6cf76f4b4e677dd04139ba6f5432f5fc11fe35b0ae_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const FString& as_ast_d3,
	const double as_ast_d4,
	const FLinearColor& as_ast_d5)
{
	{
		TypedFrame.SetSourcePosition(7, 2);
		([&]() -> void
		{
			const FLinearColor& as_ast_e5_arg2 = as_ast_d5;
			const float as_ast_e5_arg1 = static_cast<float>(as_ast_d4);
			const FString& as_ast_e5_arg0 = as_ast_d3;
			AngelscriptStaticJITNativeCallables::Print(as_ast_e5_arg0, as_ast_e5_arg1, as_ast_e5_arg2);
			return;
		}());
	}
	{
		TypedFrame.SetSourcePosition(8, 2);
		return int32(314);
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:5
// JIT Entry   : Raw
int32 ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_22520f4a76bcce5aecea9e6cf76f4b4e677dd04139ba6f5432f5fc11fe35b0ae_Raw(FScriptExecution& Execution, FString* p_0, double p_1, FLinearColor* p_2)
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
	return ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_22520f4a76bcce5aecea9e6cf76f4b4e677dd04139ba6f5432f5fc11fe35b0ae_TypedBody(TypedFrame, *p_0, static_cast<double>(p_1), *p_2);
}

// AS Function : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:5
// JIT Entry   : VM
void ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_22520f4a76bcce5aecea9e6cf76f4b4e677dd04139ba6f5432f5fc11fe35b0ae_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_22520f4a76bcce5aecea9e6cf76f4b4e677dd04139ba6f5432f5fc11fe35b0ae_Raw(Execution,
		*(FString**)(l_fp + 0),
		*(double*)(l_fp + 2),
		*(FLinearColor**)(l_fp + 4));
}

// AS Function : int TypedASTPrintShowcase(const FString&in, const float, const FLinearColor&in)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrintFixture.as:2:5
// JIT Entry   : Parms
void ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_22520f4a76bcce5aecea9e6cf76f4b4e677dd04139ba6f5432f5fc11fe35b0ae_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_9b9ea7239812ba225616d8419fc67d8fee16a911d0b51872166534a2d0ee91a4_22520f4a76bcce5aecea9e6cf76f4b4e677dd04139ba6f5432f5fc11fe35b0ae_Raw(Execution,
		(FString*)(((SIZE_T)Parms) + ParmOffset_0),
		static_cast<double>(*(double*)(((SIZE_T)Parms) + ParmOffset_1)),
		(FLinearColor*)(((SIZE_T)Parms) + ParmOffset_2));
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
