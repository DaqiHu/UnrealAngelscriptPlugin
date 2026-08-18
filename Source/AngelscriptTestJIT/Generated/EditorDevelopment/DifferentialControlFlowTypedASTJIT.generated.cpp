// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : TypedASTJIT
 * Declaration        : int SemanticExhaustiveEnum(const int)
 * StableFunctionKey  : 0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4
 * OriginalSymbolSet  : ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_52de55a9a6328e299cf7f8704913c970b8ca29f9de21e7623850392073ca7ecd
 * TestSymbolSet      : AS_Test_Differential_TypedAST_SemanticExhaustiveEnum
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
 * Declaration       : int SemanticExhaustiveEnum(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
 * StableFunctionKey  : 0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4
 * ExecutionHash      : 52de55a9a6328e299cf7f8704913c970b8ca29f9de21e7623850392073ca7ecd
 * DebugHash          : 68708e5118144807270fa93f1ddeb85a92b89ed2a131c0fed467dc787a1fbf1b
 * EntryAbiHash       : ac5e9740aa5289dba47d6e5ba3f40c6f1f11e575d2ab2615b198bbbc11568b3d
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 AS_Test_Differential_TypedAST_SemanticExhaustiveEnum_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0);

static int32 AS_Test_Differential_TypedAST_SemanticExhaustiveEnum_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0)
{
	TypedFrame.SetSourcePosition(103, 23);
	const int8 as_sem_s1 = AngelscriptTypedASTJIT::WrapNarrow<int8>(as_sem_s0);
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	const int32 as_sem_switch_selector_10 = static_cast<int32>(as_sem_s1);
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	switch (as_sem_switch_selector_10)
	{
		case int32(0):
			TypedFrame.SetSourcePosition(106, 2);
			{
				TypedFrame.SetSourcePosition(107, 3);
				return int32(10);
			}
			break;
		case int32(1):
			TypedFrame.SetSourcePosition(108, 2);
			{
				TypedFrame.SetSourcePosition(109, 3);
				return int32(20);
			}
			break;
		case int32(2):
			TypedFrame.SetSourcePosition(110, 2);
			{
				TypedFrame.SetSourcePosition(111, 3);
				return int32(30);
			}
			break;
		default:
			TypedFrame.SetSourcePosition(104, 2);
			FStaticJITFunction::SetSwitchValueInvalidException(Execution);
			return int32{};
	}
}

// AS Function : int SemanticExhaustiveEnum(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
// JIT Entry   : Raw
int32 AS_Test_Differential_TypedAST_SemanticExhaustiveEnum_Raw(FScriptExecution& Execution, asDWORD p_0)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as",
		"int SemanticExhaustiveEnum(const int)",
		101,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return AS_Test_Differential_TypedAST_SemanticExhaustiveEnum_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int SemanticExhaustiveEnum(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
// JIT Entry   : VM
void AS_Test_Differential_TypedAST_SemanticExhaustiveEnum_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_TypedAST_SemanticExhaustiveEnum_Raw(Execution,
		*(asDWORD*)(l_fp + 0));
}

// AS Function : int SemanticExhaustiveEnum(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
// JIT Entry   : Parms
void AS_Test_Differential_TypedAST_SemanticExhaustiveEnum_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_Differential_TypedAST_SemanticExhaustiveEnum_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FTypedASTJITDifferentialEntryRegistrationReferenceSlots[1] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(2), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("da204858ced604390176b13c9258e987373f2b4e798ef2bae576892d1387d4a8")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("8746c9775c41ecdcb7c63d6370d6a1a82a6d6c81fab94b0d5db855661c5b9865")))}}},
	};

	struct FTypedASTJITDifferentialEntryRegistration
	{
		FTypedASTJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::TypedASTJIT,
				&AS_Test_Differential_TypedAST_SemanticExhaustiveEnum_VM,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_TypedAST_SemanticExhaustiveEnum_Raw),
				&AS_Test_Differential_TypedAST_SemanticExhaustiveEnum_Parms,
				TEXT("AS_Test_Differential_TypedAST_SemanticExhaustiveEnum"),
				TEXT("0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4"),
				TEXT("int SemanticExhaustiveEnum(const int)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as"),
				101u,
				1u,
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
