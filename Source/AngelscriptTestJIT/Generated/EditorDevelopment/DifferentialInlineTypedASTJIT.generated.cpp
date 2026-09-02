// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : TypedASTJIT
 * Declaration        : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
 * StableFunctionKey  : a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b
 * OriginalSymbolSet  : ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_e75fe728e40dc52aa18508e8758d65b02d0c622da99b6e53f885a65d7dd31bf5
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
 * ExecutionHash      : e75fe728e40dc52aa18508e8758d65b02d0c622da99b6e53f885a65d7dd31bf5
 * DebugHash          : f1675cc2d072786fb7cbe4777d1caac574993ce75d470854da1303c4a98ee287
 * EntryAbiHash       : 315429bd98bc57e8264be8ad0e7bc5007a550d671431aa6b3c1f76f34b2aa667
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d37,
	const int32 as_ast_d38,
	const bool as_ast_d39,
	const bool as_ast_d40);

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
	const int32 as_ast_d37,
	const int32 as_ast_d38,
	const bool as_ast_d39,
	const bool as_ast_d40)
{
	int32 as_ast_d41;
	{
		TypedFrame.SetSourcePosition(56, 6);
		([&]() -> int32
		{
			const int32 as_ast_e63_value = ([&]() -> int32
		{
			const int32 as_ast_e61_left = as_ast_d37;
			const int32 as_ast_e61_right = as_ast_d38;
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e61_left, as_ast_e61_right);
		}());
			int32& as_ast_e63_target = as_ast_d41;
			as_ast_e63_target = as_ast_e63_value;
			return as_ast_e63_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(57, 2);
		([&]() -> int32
		{
			const int32 as_ast_e68_value = ([&]() -> int32
		{
			const int32 as_ast_e67_left = as_ast_d41;
			const int32 as_ast_e67_right = int32(1);
			return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e67_left, as_ast_e67_right);
		}());
			int32& as_ast_e68_target = as_ast_d41;
			as_ast_e68_target = as_ast_e68_value;
			return as_ast_e68_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(58, 2);
		([&]() -> int32
		{
			const int32 as_ast_e71_value = int32(3);
			int32& as_ast_e71_target = as_ast_d41;
			as_ast_e71_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e71_target, as_ast_e71_value);
			return as_ast_e71_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(59, 2);
		([&]() -> int32
		{
			const int32 as_ast_e74_value = int32(1);
			int32& as_ast_e74_target = as_ast_d41;
			as_ast_e74_target = AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e74_target, as_ast_e74_value);
			return as_ast_e74_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(60, 2);
		([&]() -> int32
		{
			const int32 as_ast_e77_value = int32(2);
			int32& as_ast_e77_target = as_ast_d41;
			as_ast_e77_target = AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e77_target, as_ast_e77_value);
			return as_ast_e77_target;
		}());
	}
	int32 as_ast_d42;
	{
		TypedFrame.SetSourcePosition(62, 6);
		([&]() -> int32
		{
			const int32 as_ast_e82_value = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d41);
			int32& as_ast_e82_target = as_ast_d42;
			as_ast_e82_target = as_ast_e82_value;
			return as_ast_e82_target;
		}());
	}
	int32 as_ast_d43;
	{
		TypedFrame.SetSourcePosition(63, 6);
		([&]() -> int32
		{
			const int32 as_ast_e87_value = AngelscriptTypedASTJIT::WrapPostIncrement<int32>(as_ast_d41);
			int32& as_ast_e87_target = as_ast_d43;
			as_ast_e87_target = as_ast_e87_value;
			return as_ast_e87_target;
		}());
	}
	int32 as_ast_d44;
	{
		TypedFrame.SetSourcePosition(64, 6);
		([&]() -> int32
		{
			const int32 as_ast_e91_value = AngelscriptTypedASTJIT::WrapNegate<int32>(as_ast_d42);
			int32& as_ast_e91_target = as_ast_d44;
			as_ast_e91_target = as_ast_e91_value;
			return as_ast_e91_target;
		}());
	}
	int32 as_ast_d45;
	{
		TypedFrame.SetSourcePosition(65, 6);
		([&]() -> int32
		{
			const int32 as_ast_e95_value = AngelscriptTypedASTJIT::BitwiseNot<int32>(as_ast_d43);
			int32& as_ast_e95_target = as_ast_d45;
			as_ast_e95_target = as_ast_e95_value;
			return as_ast_e95_target;
		}());
	}
	int32 as_ast_d46;
	{
		TypedFrame.SetSourcePosition(66, 6);
		([&]() -> int32
		{
			const int32 as_ast_e100_value = ([&]() -> int32
		{
			const int32 as_ast_e98_left = as_ast_d41;
			const int32 as_ast_e98_right = int32(1);
			return AngelscriptTypedASTJIT::ShiftLeft<int32>(as_ast_e98_left, as_ast_e98_right);
		}());
			int32& as_ast_e100_target = as_ast_d46;
			as_ast_e100_target = as_ast_e100_value;
			return as_ast_e100_target;
		}());
	}
	int32 as_ast_d47;
	{
		TypedFrame.SetSourcePosition(67, 6);
		([&]() -> int32
		{
			const int32 as_ast_e105_value = ([&]() -> int32
		{
			const int32 as_ast_e103_left = as_ast_d41;
			const int32 as_ast_e103_right = int32(1);
			return AngelscriptTypedASTJIT::LogicalShiftRight<int32>(as_ast_e103_left, as_ast_e103_right);
		}());
			int32& as_ast_e105_target = as_ast_d47;
			as_ast_e105_target = as_ast_e105_value;
			return as_ast_e105_target;
		}());
	}
	bool as_ast_d48;
	{
		TypedFrame.SetSourcePosition(69, 7);
		([&]() -> bool
		{
			const bool as_ast_e114_value = (as_ast_d39 && ([&]() -> bool
		{
			const int32 as_ast_e111_left = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d41);
			const int32 as_ast_e111_right = as_ast_d38;
			return (as_ast_e111_left > as_ast_e111_right);
		}()));
			bool& as_ast_e114_target = as_ast_d48;
			as_ast_e114_target = as_ast_e114_value;
			return as_ast_e114_target;
		}());
	}
	bool as_ast_d49;
	{
		TypedFrame.SetSourcePosition(70, 7);
		([&]() -> bool
		{
			const bool as_ast_e121_value = ((as_ast_d48 && as_ast_d40) && IsRunningCommandlet());
			bool& as_ast_e121_target = as_ast_d49;
			as_ast_e121_target = as_ast_e121_value;
			return as_ast_e121_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(72, 2);
		if (as_ast_d49)
		{
			{
				TypedFrame.SetSourcePosition(74, 3);
				return ([&]() -> int32
			{
				const int32 as_ast_e127_left = ([&]() -> int32
			{
				const int32 as_ast_e126_left = as_ast_d46;
				const int32 as_ast_e126_right = as_ast_d45;
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e126_left, as_ast_e126_right);
			}());
				const int32 as_ast_e127_right = as_ast_d41;
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e127_left, as_ast_e127_right);
			}());
			}
		}
	}
	{
		TypedFrame.SetSourcePosition(76, 2);
		if (([&]() -> bool
		{
			const int32 as_ast_e130_left = as_ast_d47;
			const int32 as_ast_e130_right = int32(10);
			return (as_ast_e130_left > as_ast_e130_right);
		}()))
		{
			{
				TypedFrame.SetSourcePosition(78, 3);
				return ([&]() -> int32
			{
				const int32 as_ast_e135_left = ([&]() -> int32
			{
				const int32 as_ast_e134_left = as_ast_d47;
				const int32 as_ast_e134_right = int32(2);
				return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e134_left, as_ast_e134_right);
			}());
				const int32 as_ast_e135_right = as_ast_d41;
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e135_left, as_ast_e135_right);
			}());
			}
		}
	}
	{
		TypedFrame.SetSourcePosition(80, 2);
		return ([&]() -> int32
	{
		const int32 as_ast_e140_left = ([&]() -> int32
	{
		const int32 as_ast_e139_left = as_ast_d44;
		const int32 as_ast_e139_right = int32(1);
		return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e139_left, as_ast_e139_right);
	}());
		const int32 as_ast_e140_right = as_ast_d41;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e140_left, as_ast_e140_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
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
