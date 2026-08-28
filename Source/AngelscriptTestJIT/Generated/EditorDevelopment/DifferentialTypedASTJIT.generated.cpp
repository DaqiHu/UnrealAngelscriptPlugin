// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : TypedASTJIT
 * Declaration        : int DifferentialScalarValue(const int, const int)
 * StableFunctionKey  : bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b
 * OriginalSymbolSet  : ASJIT_bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b_df81f198ae9cc8880f4b4e416ec96091ac1043eb47e016199fc5636cc2f4cac1
 * TestSymbolSet      : AS_Test_Differential_TypedAST_DifferentialScalarValue
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
static int32 AS_Test_Differential_TypedAST_DifferentialScalarValue_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d9,
	const int32 as_ast_d10);

static int32 AS_Test_Differential_TypedAST_DifferentialScalarValue_TypedBody(
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
int32 AS_Test_Differential_TypedAST_DifferentialScalarValue_Raw(FScriptExecution& Execution, void* Object, asDWORD p_0, asDWORD p_1)
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
	return AS_Test_Differential_TypedAST_DifferentialScalarValue_TypedBody(TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int DifferentialScalarValue(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:6
// JIT Entry   : VM
void AS_Test_Differential_TypedAST_DifferentialScalarValue_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_TypedAST_DifferentialScalarValue_Raw(Execution,
		*(void**)l_fp,
		*(asDWORD*)(l_fp + 2),
		*(asDWORD*)(l_fp + 3));
}

// AS Function : int DifferentialScalarValue(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:14:6
// JIT Entry   : Parms
void AS_Test_Differential_TypedAST_DifferentialScalarValue_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_Differential_TypedAST_DifferentialScalarValue_Raw(Execution,
		Object,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FTypedASTJITDifferentialEntryRegistrationReferenceSlots[1] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(2), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("fad89b1b314e72fddfda397e555d92b3e881dce5158330451090697253e7b4e8")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("85b780d3d0320f0320f21f66806418c53611e2e04d160beb79f3c8a598f7a9bd")))}}},
	};

	struct FTypedASTJITDifferentialEntryRegistration
	{
		FTypedASTJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::TypedASTJIT,
				&AS_Test_Differential_TypedAST_DifferentialScalarValue_VM,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_TypedAST_DifferentialScalarValue_Raw),
				&AS_Test_Differential_TypedAST_DifferentialScalarValue_Parms,
				TEXT("AS_Test_Differential_TypedAST_DifferentialScalarValue"),
				TEXT("bcc16a42d6581e28f5fd897a3719e70c37469f91213349e84390b2ae478ef35b"),
				TEXT("int DifferentialScalarValue(const int, const int)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as"),
				14u,
				6u,
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
