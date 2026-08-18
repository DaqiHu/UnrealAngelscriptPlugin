// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : TypedASTJIT
 * Declaration        : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
 * StableFunctionKey  : a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b
 * OriginalSymbolSet  : ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872
 * TestSymbolSet      : AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase
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
 * Declaration       : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
 * StableFunctionKey  : a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b
 * ExecutionHash      : f7ee08ffb0af7fd6427bebd4c9dfc21fb9ec36a2a04aabc61d7c2a60a1097872
 * DebugHash          : b13cf30324227b9a171896dc6287854a35be6e5d2dfefde39170521d32f42fb8
 * EntryAbiHash       : 315429bd98bc57e8264be8ad0e7bc5007a550d671431aa6b3c1f76f34b2aa667
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1,
	const bool as_sem_s2,
	const bool as_sem_s3);

/*
 * AngelScript Native Call Site 0
 * AS Declaration      : bool IsRunningCommandlet()
 * Emitted C++ Callee  : IsRunningCommandlet
 * Route               : DirectInline
 * Effective Target    : IsRunningCommandlet
 * Runtime DLL Core    : <none>
 * Metadata Row        : <none>
 * Reference Slot      : -1
 */
static int32 AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_sem_s0,
	const int32 as_sem_s1,
	const bool as_sem_s2,
	const bool as_sem_s3)
{
	TypedFrame.SetSourcePosition(56, 6);
	int32 as_sem_s4 = ([&]() -> int32
	{
		const int32 as_sem_e2_left = as_sem_s0;
		const int32 as_sem_e2_right = as_sem_s1;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e2_left, as_sem_e2_right);
	}());
	TypedFrame.SetSourcePosition(57, 2);
	([&]() -> int32
	{
		const int32 as_sem_e7_value = ([&]() -> int32
		{
			const int32 as_sem_e5_left = as_sem_s4;
			const int32 as_sem_e5_right = int32(1);
			return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_sem_e5_left, as_sem_e5_right);
		}());
		int32& as_sem_e7_target = as_sem_s4;
		as_sem_e7_target = as_sem_e7_value;
		return as_sem_e7_target;
	}());
	TypedFrame.SetSourcePosition(58, 2);
	([&]() -> int32
	{
		const int32 as_sem_e10_value = int32(3);
		int32& as_sem_e10_target = as_sem_s4;
		as_sem_e10_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e10_target, as_sem_e10_value);
		return as_sem_e10_target;
	}());
	TypedFrame.SetSourcePosition(59, 2);
	([&]() -> int32
	{
		const int32 as_sem_e13_value = int32(1);
		int32& as_sem_e13_target = as_sem_s4;
		as_sem_e13_target = AngelscriptTypedASTJIT::WrapSubtract<int32>(as_sem_e13_target, as_sem_e13_value);
		return as_sem_e13_target;
	}());
	TypedFrame.SetSourcePosition(60, 2);
	([&]() -> int32
	{
		const int32 as_sem_e16_value = int32(2);
		int32& as_sem_e16_target = as_sem_s4;
		as_sem_e16_target = AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e16_target, as_sem_e16_value);
		return as_sem_e16_target;
	}());
	TypedFrame.SetSourcePosition(62, 6);
	const int32 as_sem_s5 = ([&]() -> int32
	{
		int32& as_sem_e18_target = as_sem_s4;
		as_sem_e18_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e18_target, int32(1));
		return as_sem_e18_target;
	}());
	TypedFrame.SetSourcePosition(63, 6);
	const int32 as_sem_s6 = ([&]() -> int32
	{
		int32& as_sem_e20_target = as_sem_s4;
		const int32 as_sem_e20_old = as_sem_e20_target;
		as_sem_e20_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e20_old, int32(1));
		return as_sem_e20_old;
	}());
	TypedFrame.SetSourcePosition(64, 6);
	const int32 as_sem_s7 = AngelscriptTypedASTJIT::WrapNegate<int32>(as_sem_s5);
	TypedFrame.SetSourcePosition(65, 6);
	const int32 as_sem_s8 = AngelscriptTypedASTJIT::BitwiseNot<int32>(as_sem_s6);
	TypedFrame.SetSourcePosition(66, 6);
	const int32 as_sem_s9 = ([&]() -> int32
	{
		const int32 as_sem_e28_left = as_sem_s4;
		const uint32 as_sem_e28_right = AngelscriptTypedASTJIT::ConvertInteger<uint32>(int32(1));
		return AngelscriptTypedASTJIT::ShiftLeft<int32>(as_sem_e28_left, as_sem_e28_right);
	}());
	TypedFrame.SetSourcePosition(67, 6);
	const int32 as_sem_s10 = ([&]() -> int32
	{
		const int32 as_sem_e32_left = as_sem_s4;
		const uint32 as_sem_e32_right = AngelscriptTypedASTJIT::ConvertInteger<uint32>(int32(1));
		return AngelscriptTypedASTJIT::LogicalShiftRight<int32>(as_sem_e32_left, as_sem_e32_right);
	}());
	TypedFrame.SetSourcePosition(69, 7);
	const bool as_sem_s11 = (as_sem_s2 && ([&]() -> bool
	{
		const int32 as_sem_e37_left = ([&]() -> int32
	{
		int32& as_sem_e35_target = as_sem_s4;
		as_sem_e35_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e35_target, int32(1));
		return as_sem_e35_target;
	}());
		const int32 as_sem_e37_right = as_sem_s1;
		return (as_sem_e37_left > as_sem_e37_right);
	}()));
	TypedFrame.SetSourcePosition(70, 7);
	const bool as_sem_s12 = ((as_sem_s11 && as_sem_s3) && ([&]() -> bool
	{
		TypedFrame.SetSourcePosition(71, 35);
		return IsRunningCommandlet();
	}()));
	TypedFrame.SetSourcePosition(72, 2);
	if (as_sem_s12)
	{
		TypedFrame.SetSourcePosition(74, 3);
		return ([&]() -> int32
		{
			const int32 as_sem_e49_left = ([&]() -> int32
		{
			const int32 as_sem_e47_left = as_sem_s9;
			const int32 as_sem_e47_right = as_sem_s8;
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e47_left, as_sem_e47_right);
		}());
			const int32 as_sem_e49_right = as_sem_s4;
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e49_left, as_sem_e49_right);
		}());
	}
	TypedFrame.SetSourcePosition(76, 2);
	if (([&]() -> bool
	{
		const int32 as_sem_e52_left = as_sem_s10;
		const int32 as_sem_e52_right = int32(10);
		return (as_sem_e52_left > as_sem_e52_right);
	}()))
	{
		TypedFrame.SetSourcePosition(78, 3);
		return ([&]() -> int32
		{
			const int32 as_sem_e57_left = ([&]() -> int32
		{
			const int32 as_sem_e55_left = as_sem_s10;
			const int32 as_sem_e55_right = int32(2);
			return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e55_left, as_sem_e55_right);
		}());
			const int32 as_sem_e57_right = as_sem_s4;
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e57_left, as_sem_e57_right);
		}());
	}
	TypedFrame.SetSourcePosition(80, 2);
	return ([&]() -> int32
	{
		const int32 as_sem_e62_left = ([&]() -> int32
	{
		const int32 as_sem_e60_left = as_sem_s7;
		const int32 as_sem_e60_right = int32(1);
		return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_sem_e60_left, as_sem_e60_right);
	}());
		const int32 as_sem_e62_right = as_sem_s4;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e62_left, as_sem_e62_right);
	}());
}

// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
// JIT Entry   : Raw
int32 AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1, asBYTE p_2, asBYTE p_3)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITAotFixture.as",
		"int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)",
		50,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase_TypedBody(TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1), static_cast<bool>(p_2), static_cast<bool>(p_3));
}

// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
// JIT Entry   : VM
void AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1),
		*(asBYTE*)(l_fp + 2),
		*(asBYTE*)(l_fp + 3));
}

// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:1
// JIT Entry   : Parms
void AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase_Parms(FScriptExecution& Execution, void* Object, void* Parms)
{
	static_cast<void>(Object);
	SIZE_T ParmsOffset = 0;
	const SIZE_T ParmOffset_0 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ParmOffset_1 = ParmsOffset;
	ParmsOffset += sizeof(int32);
	ParmsOffset = Align(ParmsOffset, alignof(bool));
	const SIZE_T ParmOffset_2 = ParmsOffset;
	ParmsOffset += sizeof(bool);
	ParmsOffset = Align(ParmsOffset, alignof(bool));
	const SIZE_T ParmOffset_3 = ParmsOffset;
	ParmsOffset += sizeof(bool);
	ParmsOffset = Align(ParmsOffset, alignof(void*));
	const SIZE_T GeneratedWorldContextParmOffset = ParmsOffset;
	ParmsOffset += sizeof(void*);
	static_cast<void>(GeneratedWorldContextParmOffset);
	ParmsOffset = Align(ParmsOffset, alignof(int32));
	const SIZE_T ReturnParmOffset = ParmsOffset;
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)),
		static_cast<asBYTE>(*(bool*)(((SIZE_T)Parms) + ParmOffset_2)),
		static_cast<asBYTE>(*(bool*)(((SIZE_T)Parms) + ParmOffset_3)));
}

namespace
{
	static const FAngelscriptJITReferenceSlot FTypedASTJITDifferentialEntryRegistrationReferenceSlots[1] =
	{
		{sizeof(FAngelscriptJITReferenceSlot), 0u, static_cast<EAngelscriptJITReferenceSlotFlags>(0), 0u, {static_cast<EAngelscriptArtifactReferenceKind>(7), FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("d29d598a9a8a090fd329db0c1d3fe9a9feaa707f0f7f52e7c6ed766e9a0324fe")))}, FAngelscriptHash256{FBlake3Hash(FWideStringView(TEXT("fbe6e43e9bfbf5b301c74de7f09a0baea8de4212da0cbc901a1e39c68e1b8eb5")))}}},
	};

	struct FTypedASTJITDifferentialEntryRegistration
	{
		FTypedASTJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::TypedASTJIT,
				&AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase_VM,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase_Raw),
				&AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase_Parms,
				TEXT("AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase"),
				TEXT("a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b"),
				TEXT("int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)"),
				TEXT("/Angelscript/Game/ASStaticJITAotFixture.as"),
				50u,
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
