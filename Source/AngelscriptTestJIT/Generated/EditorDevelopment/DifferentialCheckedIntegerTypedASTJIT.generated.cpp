// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : TypedASTJIT
 * Declaration        : int SemanticCheckedDivide(const int, const int)
 * StableFunctionKey  : a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5
 * OriginalSymbolSet  : ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_3c9395cb48100816600076228d1517860b80fdd10e52295aac2a31f1b9af6f3d
 * TestSymbolSet      : AS_Test_Differential_TypedAST_SemanticCheckedDivide
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
 * Declaration       : int SemanticCheckedDivide(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
 * StableFunctionKey  : a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5
 * ExecutionHash      : 3c9395cb48100816600076228d1517860b80fdd10e52295aac2a31f1b9af6f3d
 * DebugHash          : 7be5fa7ce3d293b37a860e95bb2fc9f0448c71efd8c89d5f7d190ea0fd86bf06
 * EntryAbiHash       : 34574e76e092ff59a9ab137303c6c8a1a00f50e50bf9ce931ed07453463297ff
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 AS_Test_Differential_TypedAST_SemanticCheckedDivide_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1);

static int32 AS_Test_Differential_TypedAST_SemanticCheckedDivide_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1)
{
	TypedFrame.SetSourcePosition(118, 2);
	return ([&]() -> int32
	{
		const int32 as_sem_e2_left = as_sem_s0;
		const int32 as_sem_e2_right = as_sem_s1;
		return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e2_left, as_sem_e2_right);
	}());
}

// AS Function : int SemanticCheckedDivide(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
// JIT Entry   : Raw
int32 AS_Test_Differential_TypedAST_SemanticCheckedDivide_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as",
		"int SemanticCheckedDivide(const int, const int)",
		116,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return AS_Test_Differential_TypedAST_SemanticCheckedDivide_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int SemanticCheckedDivide(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
// JIT Entry   : VM
void AS_Test_Differential_TypedAST_SemanticCheckedDivide_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_TypedAST_SemanticCheckedDivide_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int SemanticCheckedDivide(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
// JIT Entry   : Parms
void AS_Test_Differential_TypedAST_SemanticCheckedDivide_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_Differential_TypedAST_SemanticCheckedDivide_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}

namespace
{
	struct FTypedASTJITDifferentialEntryRegistration
	{
		FTypedASTJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::TypedASTJIT,
				&AS_Test_Differential_TypedAST_SemanticCheckedDivide_VM,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_TypedAST_SemanticCheckedDivide_Raw),
				&AS_Test_Differential_TypedAST_SemanticCheckedDivide_Parms,
				TEXT("AS_Test_Differential_TypedAST_SemanticCheckedDivide"),
				TEXT("a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5"),
				TEXT("int SemanticCheckedDivide(const int, const int)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as"),
				116u,
				1u,
				AngelscriptTestJITProbes::EIsolatedDifferentialCleanupPlanState::VerifiedEmpty,
				0u,
				nullptr,
				0u);
		}
	};

	FTypedASTJITDifferentialEntryRegistration GTypedASTJITDifferentialEntryRegistration;
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
