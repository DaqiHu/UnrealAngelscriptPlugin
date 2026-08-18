// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : TypedASTJIT
 * Declaration        : int SemanticNumericBoundary(const int, const int, const int, const int)
 * StableFunctionKey  : de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e
 * OriginalSymbolSet  : ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_58539743e28e50faa5d5ee3e50261836e051b1dd0a7d68b805bab6072fd2d80a
 * TestSymbolSet      : AS_Test_Differential_TypedAST_SemanticNumericBoundary
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
 * Declaration       : int SemanticNumericBoundary(const int, const int, const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
 * StableFunctionKey  : de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e
 * ExecutionHash      : 58539743e28e50faa5d5ee3e50261836e051b1dd0a7d68b805bab6072fd2d80a
 * DebugHash          : 33ab9e7722bfe134d36a9e683a832e75cb1a6a5322a13ab18e7bd23d86aea50f
 * EntryAbiHash       : ce69b03c30f4aba54525c218c4551a91f6dfe1f289fefa74e6e6ea01b1bab739
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 AS_Test_Differential_TypedAST_SemanticNumericBoundary_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s1,
	const int32 as_sem_s2,
	const int32 as_sem_s3,
	const int32 as_sem_s4);

static int32 AS_Test_Differential_TypedAST_SemanticNumericBoundary_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s1,
	const int32 as_sem_s2,
	const int32 as_sem_s3,
	const int32 as_sem_s4)
{
	TypedFrame.SetSourcePosition(32, 3);
	const int32 as_sem_switch_selector_43 = static_cast<int32>(as_sem_s4);
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	switch (as_sem_switch_selector_43)
	{
		case int32(0):
			TypedFrame.SetSourcePosition(34, 3);
			{
				TypedFrame.SetSourcePosition(35, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e14_left = as_sem_s1;
					const int32 as_sem_e14_right = as_sem_s2;
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e14_left, as_sem_e14_right);
				}());
			}
			break;
		case int32(1):
			TypedFrame.SetSourcePosition(36, 3);
			{
				TypedFrame.SetSourcePosition(37, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e17_left = as_sem_s1;
					const int32 as_sem_e17_right = as_sem_s2;
					return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_sem_e17_left, as_sem_e17_right);
				}());
			}
			break;
		case int32(2):
			TypedFrame.SetSourcePosition(38, 3);
			{
				TypedFrame.SetSourcePosition(39, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e20_left = as_sem_s1;
					const int32 as_sem_e20_right = as_sem_s2;
					return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e20_left, as_sem_e20_right);
				}());
			}
			break;
		case int32(3):
			TypedFrame.SetSourcePosition(40, 3);
			{
				TypedFrame.SetSourcePosition(41, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e23_left = as_sem_s1;
					const int32 as_sem_e23_right = as_sem_s2;
					return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_sem_e23_left, as_sem_e23_right);
				}());
			}
			break;
		case int32(4):
			TypedFrame.SetSourcePosition(42, 3);
			{
				TypedFrame.SetSourcePosition(43, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e26_left = as_sem_s1;
					const int32 as_sem_e26_right = as_sem_s2;
					return AngelscriptTypedASTJIT::CheckedRemainder<int32>(Execution, as_sem_e26_left, as_sem_e26_right);
				}());
			}
			break;
		case int32(5):
			TypedFrame.SetSourcePosition(44, 3);
			{
				TypedFrame.SetSourcePosition(45, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e30_left = as_sem_s1;
					const uint32 as_sem_e30_right = AngelscriptTypedASTJIT::ConvertInteger<uint32>(as_sem_s3);
					return AngelscriptTypedASTJIT::ShiftLeft<int32>(as_sem_e30_left, as_sem_e30_right);
				}());
			}
			break;
		case int32(6):
			TypedFrame.SetSourcePosition(46, 3);
			{
				TypedFrame.SetSourcePosition(47, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e34_left = as_sem_s1;
					const uint32 as_sem_e34_right = AngelscriptTypedASTJIT::ConvertInteger<uint32>(as_sem_s3);
					return AngelscriptTypedASTJIT::LogicalShiftRight<int32>(as_sem_e34_left, as_sem_e34_right);
				}());
			}
			break;
		case int32(7):
			TypedFrame.SetSourcePosition(48, 3);
			{
				TypedFrame.SetSourcePosition(49, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e38_left = as_sem_s1;
					const uint32 as_sem_e38_right = AngelscriptTypedASTJIT::ConvertInteger<uint32>(as_sem_s3);
					return AngelscriptTypedASTJIT::ArithmeticShiftRight<int32>(as_sem_e38_left, as_sem_e38_right);
				}());
			}
			break;
		case int32(8):
			TypedFrame.SetSourcePosition(50, 3);
			{
				TypedFrame.SetSourcePosition(52, 10);
				const int64 as_sem_s5 = static_cast<int64>(as_sem_s1);
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				TypedFrame.SetSourcePosition(53, 4);
				const bool as_sem_if_condition_27 = ([&]() -> bool
				{
					const int64 as_sem_e44_left = as_sem_s5;
					const int64 as_sem_e44_right = static_cast<int64>(as_sem_s1);
					return (as_sem_e44_left == as_sem_e44_right);
				}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				if (as_sem_if_condition_27)
				{
					TypedFrame.SetSourcePosition(55, 5);
					return as_sem_s1;
				}
				TypedFrame.SetSourcePosition(57, 4);
				return int32(0);
			}
			break;
		case int32(9):
			TypedFrame.SetSourcePosition(59, 3);
			{
				TypedFrame.SetSourcePosition(60, 4);
				const bool as_sem_if_condition_33 = ([&]() -> bool
				{
					const int32 as_sem_e49_left = as_sem_s1;
					const int32 as_sem_e49_right = int32(0);
					return (as_sem_e49_left != as_sem_e49_right);
				}());
				if (UNLIKELY(Execution.bExceptionThrown))
				{
					return int32{};
				}
				if (as_sem_if_condition_33)
				{
					TypedFrame.SetSourcePosition(62, 5);
					return int32(1);
				}
				TypedFrame.SetSourcePosition(64, 4);
				return int32(0);
			}
			break;
		case int32(10):
			TypedFrame.SetSourcePosition(65, 3);
			{
				TypedFrame.SetSourcePosition(66, 4);
				return ([&]() -> int32
				{
					const int32 as_sem_e54_left = AngelscriptTypedASTJIT::FromCanonicalBits<int32>(UINT64_C(7001));
					const int32 as_sem_e54_right = as_sem_s1;
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e54_left, as_sem_e54_right);
				}());
			}
			break;
		default:
			TypedFrame.SetSourcePosition(67, 3);
			{
				TypedFrame.SetSourcePosition(68, 4);
				return AngelscriptTypedASTJIT::WrapNegate<int32>(int32(1));
			}
			break;
	}
}

// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
// JIT Entry   : Raw
int32 AS_Test_Differential_TypedAST_SemanticNumericBoundary_Raw(FScriptExecution& Execution, void* Object, asDWORD p_0, asDWORD p_1, asDWORD p_2, asDWORD p_3)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as",
		"int SemanticNumericBoundary(const int, const int, const int, const int)",
		26,
		Object,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return AS_Test_Differential_TypedAST_SemanticNumericBoundary_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1), static_cast<int32>(p_2), static_cast<int32>(p_3));
}

// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
// JIT Entry   : VM
void AS_Test_Differential_TypedAST_SemanticNumericBoundary_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_TypedAST_SemanticNumericBoundary_Raw(Execution,
		*(void**)l_fp,
		*(asDWORD*)(l_fp + 2),
		*(asDWORD*)(l_fp + 3),
		*(asDWORD*)(l_fp + 4),
		*(asDWORD*)(l_fp + 5));
}

// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:2
// JIT Entry   : Parms
void AS_Test_Differential_TypedAST_SemanticNumericBoundary_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_2 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_3 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_Differential_TypedAST_SemanticNumericBoundary_Raw(Execution,
		Object,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_2)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_3)));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FTypedASTJITDifferentialEntryRegistrationReferenceSlots[2] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(2), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("fad89b1b314e72fddfda397e555d92b3e881dce5158330451090697253e7b4e8")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("85b780d3d0320f0320f21f66806418c53611e2e04d160beb79f3c8a598f7a9bd")))}}},
		{sizeof(FAngelscriptJITReferenceSlot), 1u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(4), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("a5da6844fd295dd05d5a22cf9b50196a15b6cea4f30ae1fdebd1d0bac4766088")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("e7ef2b212521fee9013b2a9d94e54b7c7f48664d236c66b55651dc822f29b690")))}}},
	};

	struct FTypedASTJITDifferentialEntryRegistration
	{
		FTypedASTJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::TypedASTJIT,
				&AS_Test_Differential_TypedAST_SemanticNumericBoundary_VM,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_TypedAST_SemanticNumericBoundary_Raw),
				&AS_Test_Differential_TypedAST_SemanticNumericBoundary_Parms,
				TEXT("AS_Test_Differential_TypedAST_SemanticNumericBoundary"),
				TEXT("de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e"),
				TEXT("int SemanticNumericBoundary(const int, const int, const int, const int)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as"),
				26u,
				2u,
				AngelscriptTestJITProbes::EIsolatedDifferentialCleanupPlanState::VerifiedEmpty,
				0u,
				FTypedASTJITDifferentialEntryRegistrationReferenceSlots,
				2u);
		}
	};

	FTypedASTJITDifferentialEntryRegistration GTypedASTJITDifferentialEntryRegistration;
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
