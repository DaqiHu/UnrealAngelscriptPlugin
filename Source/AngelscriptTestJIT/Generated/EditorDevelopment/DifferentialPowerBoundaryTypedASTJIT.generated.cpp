// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : TypedASTJIT
 * Declaration        : float SemanticPowerBoundary(const float, const float)
 * StableFunctionKey  : 23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67
 * OriginalSymbolSet  : ASJIT_23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67_b949dc71a767ba38728abfed5206f2c8c6158a7a0a8f4605d6d47f473c3e3ffa
 * TestSymbolSet      : AS_Test_Differential_TypedAST_SemanticPowerBoundary
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
static double AS_Test_Differential_TypedAST_SemanticPowerBoundary_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const double as_ast_d25,
	const double as_ast_d26);

static double AS_Test_Differential_TypedAST_SemanticPowerBoundary_TypedBody(
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
		const double as_ast_e70_left = as_ast_d25;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		const double as_ast_e70_right = as_ast_d26;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		return AngelscriptTypedASTJIT::CheckedPower<double>(Execution, as_ast_e70_left, as_ast_e70_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return double{};
}

// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:10
// JIT Entry   : Raw
double AS_Test_Differential_TypedAST_SemanticPowerBoundary_Raw(FScriptExecution& Execution, void* Object, double p_0, double p_1)
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
	return AS_Test_Differential_TypedAST_SemanticPowerBoundary_TypedBody(Execution, TypedFrame, static_cast<double>(p_0), static_cast<double>(p_1));
}

// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:10
// JIT Entry   : VM
void AS_Test_Differential_TypedAST_SemanticPowerBoundary_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(double*)l_outValue = AS_Test_Differential_TypedAST_SemanticPowerBoundary_Raw(Execution,
		*(void**)l_fp,
		*(double*)(l_fp + 2),
		*(double*)(l_fp + 4));
}

// AS Function : float SemanticPowerBoundary(const float, const float)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:73:10
// JIT Entry   : Parms
void AS_Test_Differential_TypedAST_SemanticPowerBoundary_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(double);
	ParmsOffset = Align(ParmsOffset, alignof(double));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(double*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_Differential_TypedAST_SemanticPowerBoundary_Raw(Execution,
		Object,
		static_cast<double>(*(double*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<double>(*(double*)(((SIZE_T)Parms) + ParmOffset_1)));
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
				&AS_Test_Differential_TypedAST_SemanticPowerBoundary_VM,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_TypedAST_SemanticPowerBoundary_Raw),
				&AS_Test_Differential_TypedAST_SemanticPowerBoundary_Parms,
				TEXT("AS_Test_Differential_TypedAST_SemanticPowerBoundary"),
				TEXT("23f1e6dec794e6d7cc42917a3dd1fc6f8bae9cee3cb91a0bb0338624ad3b6b67"),
				TEXT("float SemanticPowerBoundary(const float, const float)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as"),
				73u,
				10u,
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
