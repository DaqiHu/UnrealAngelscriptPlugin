// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : TypedASTJIT
 * Declaration        : int TypedASTPrivateBridgeShowcase(const int, const int)
 * StableFunctionKey  : 6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e
 * OriginalSymbolSet  : ASJIT_6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e_1ff0682c38f7a2216be76ae289956859b5e29ca7121a84dcfcd2a29f9e10fc5d
 * TestSymbolSet      : AS_Test_Differential_TypedAST_TypedASTPrivateBridgeShowcase
 * CleanupPlan       : VerifiedEmpty (0 actions)
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
 * Declaration       : int TypedASTPrivateBridgeShowcase(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:2:5
 * StableFunctionKey  : 6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e
 * ExecutionHash      : 1ff0682c38f7a2216be76ae289956859b5e29ca7121a84dcfcd2a29f9e10fc5d
 * DebugHash          : a299b2efa48d37e5e4fa1b8480f8666cd52e85b0c6aca9497886ea102d10cbaa
 * EntryAbiHash       : 32c0c27f47fe1029322c0ce7e17536b36a031cc3ce9c01a4ee37be56f1b14a15
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 AS_Test_Differential_TypedAST_TypedASTPrivateBridgeShowcase_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d3,
	const int32 as_ast_d4);

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
static int32 AS_Test_Differential_TypedAST_TypedASTPrivateBridgeShowcase_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d3,
	const int32 as_ast_d4)
{
	{
		TypedFrame.SetSourcePosition(4, 2);
		return ([&]() -> int32
	{
		const int32 as_ast_e3_arg1 = as_ast_d4;
		const int32 as_ast_e3_arg0 = as_ast_d3;
		const int32 as_ast_e3_result = AngelscriptTypedASTJIT::InvokeBoundNative<int32, int32, int32>(Execution, ASJIT_Call_TypedASTProviderPrivateAdd_6b00a476_6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e, as_ast_e3_arg0, as_ast_e3_arg1);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return as_ast_e3_result;
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int TypedASTPrivateBridgeShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:2:5
// JIT Entry   : Raw
int32 AS_Test_Differential_TypedAST_TypedASTPrivateBridgeShowcase_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
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
	return AS_Test_Differential_TypedAST_TypedASTPrivateBridgeShowcase_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int TypedASTPrivateBridgeShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:2:5
// JIT Entry   : VM
void AS_Test_Differential_TypedAST_TypedASTPrivateBridgeShowcase_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_TypedAST_TypedASTPrivateBridgeShowcase_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int TypedASTPrivateBridgeShowcase(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as:2:5
// JIT Entry   : Parms
void AS_Test_Differential_TypedAST_TypedASTPrivateBridgeShowcase_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_Differential_TypedAST_TypedASTPrivateBridgeShowcase_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FTypedASTJITDifferentialEntryRegistrationReferenceSlots[1] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("6b00a476bd57a82fb216373cb0eef27d00c4acb64af32b342c47c9b7dee91aac")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("304fbe10a2078ab14e2f67d70c35fd3322a0e9ce8b25e1915cebe00e271b6e96")))}}},
	};

	struct FTypedASTJITDifferentialEntryRegistration
	{
		FTypedASTJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::TypedASTJIT,
				&AS_Test_Differential_TypedAST_TypedASTPrivateBridgeShowcase_VM,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_TypedAST_TypedASTPrivateBridgeShowcase_Raw),
				&AS_Test_Differential_TypedAST_TypedASTPrivateBridgeShowcase_Parms,
				TEXT("AS_Test_Differential_TypedAST_TypedASTPrivateBridgeShowcase"),
				TEXT("6608a6a94c2205850a5fb4a8b02ab7c08d63ec5722236ff400d6d9d50cd0625e"),
				TEXT("int TypedASTPrivateBridgeShowcase(const int, const int)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedPrivateBridgeFixture.as"),
				2u,
				5u,
				AngelscriptTestJITProbes::EIsolatedDifferentialCleanupPlanState::VerifiedEmpty,
				0u,
				FTypedASTJITDifferentialEntryRegistrationReferenceSlots,
				1u);
		}
	};

	FTypedASTJITDifferentialEntryRegistration GTypedASTJITDifferentialEntryRegistration;
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
