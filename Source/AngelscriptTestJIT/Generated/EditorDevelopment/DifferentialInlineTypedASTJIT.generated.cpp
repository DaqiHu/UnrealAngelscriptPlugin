// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : TypedASTJIT
 * Declaration        : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
 * StableFunctionKey  : a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b
 * OriginalSymbolSet  : ASJIT_a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b_18d15b8d5bd3f4789e921a45ff54539d9afce9dffd62cb44825a273ffda2328f
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
 * Source             : /Angelscript/Game/ASStaticJITAotFixture.as:50:5
 * StableFunctionKey  : a0a3a72e853494a38f163859b7f08309652a533ad47d19f3c50819091312c67b
 * ExecutionHash      : 18d15b8d5bd3f4789e921a45ff54539d9afce9dffd62cb44825a273ffda2328f
 * DebugHash          : 8bf65cc1e4776ade1eddfe8d564cb923a8b581a4b6b32e04bb343fb30a2638a0
 * EntryAbiHash       : 315429bd98bc57e8264be8ad0e7bc5007a550d671431aa6b3c1f76f34b2aa667
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 AS_Test_Differential_TypedAST_TypedASTCapabilityShowcase_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d40,
	const int32 as_ast_d41,
	const bool as_ast_d42,
	const bool as_ast_d43);

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
	const int32 as_ast_d40,
	const int32 as_ast_d41,
	const bool as_ast_d42,
	const bool as_ast_d43)
{
	int32 as_ast_d44;
	{
		TypedFrame.SetSourcePosition(56, 6);
		([&]() -> int32
		{
			const int32 as_ast_e73_value = ([&]() -> int32
		{
			const int32 as_ast_e71_left = as_ast_d40;
			const int32 as_ast_e71_right = as_ast_d41;
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e71_left, as_ast_e71_right);
		}());
			int32& as_ast_e73_target = as_ast_d44;
			as_ast_e73_target = as_ast_e73_value;
			return as_ast_e73_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(57, 2);
		([&]() -> int32
		{
			const int32 as_ast_e79_value = ([&]() -> int32
		{
			const int32 as_ast_e77_left = as_ast_d44;
			const int32 as_ast_e77_right = int32(1);
			return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e77_left, as_ast_e77_right);
		}());
			int32& as_ast_e79_target = as_ast_d44;
			as_ast_e79_target = as_ast_e79_value;
			return as_ast_e79_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(58, 2);
		([&]() -> int32
		{
			const int32 as_ast_e86_value = int32(3);
			int32& as_ast_e86_target = as_ast_d44;
			as_ast_e86_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e86_target, as_ast_e86_value);
			return as_ast_e86_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(59, 2);
		([&]() -> int32
		{
			const int32 as_ast_e91_value = int32(1);
			int32& as_ast_e91_target = as_ast_d44;
			as_ast_e91_target = AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e91_target, as_ast_e91_value);
			return as_ast_e91_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(60, 2);
		([&]() -> int32
		{
			const int32 as_ast_e96_value = int32(2);
			int32& as_ast_e96_target = as_ast_d44;
			as_ast_e96_target = AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e96_target, as_ast_e96_value);
			return as_ast_e96_target;
		}());
	}
	int32 as_ast_d45;
	{
		TypedFrame.SetSourcePosition(62, 6);
		([&]() -> int32
		{
			const int32 as_ast_e102_value = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d44);
			int32& as_ast_e102_target = as_ast_d45;
			as_ast_e102_target = as_ast_e102_value;
			return as_ast_e102_target;
		}());
	}
	int32 as_ast_d46;
	{
		TypedFrame.SetSourcePosition(63, 6);
		([&]() -> int32
		{
			const int32 as_ast_e107_value = (as_ast_d44, AngelscriptTypedASTJIT::WrapPostIncrement<int32>(as_ast_d44));
			int32& as_ast_e107_target = as_ast_d46;
			as_ast_e107_target = as_ast_e107_value;
			return as_ast_e107_target;
		}());
	}
	int32 as_ast_d47;
	{
		TypedFrame.SetSourcePosition(64, 6);
		([&]() -> int32
		{
			const int32 as_ast_e111_value = AngelscriptTypedASTJIT::WrapNegate<int32>(as_ast_d45);
			int32& as_ast_e111_target = as_ast_d47;
			as_ast_e111_target = as_ast_e111_value;
			return as_ast_e111_target;
		}());
	}
	int32 as_ast_d48;
	{
		TypedFrame.SetSourcePosition(65, 6);
		([&]() -> int32
		{
			const int32 as_ast_e115_value = AngelscriptTypedASTJIT::BitwiseNot<int32>(as_ast_d46);
			int32& as_ast_e115_target = as_ast_d48;
			as_ast_e115_target = as_ast_e115_value;
			return as_ast_e115_target;
		}());
	}
	int32 as_ast_d49;
	{
		TypedFrame.SetSourcePosition(66, 6);
		([&]() -> int32
		{
			const int32 as_ast_e121_value = ([&]() -> int32
		{
			const int32 as_ast_e118_left = as_ast_d44;
			const int32 as_ast_e118_right = int32(1);
			return AngelscriptTypedASTJIT::ShiftLeft<int32>(as_ast_e118_left, as_ast_e118_right);
		}());
			int32& as_ast_e121_target = as_ast_d49;
			as_ast_e121_target = as_ast_e121_value;
			return as_ast_e121_target;
		}());
	}
	int32 as_ast_d50;
	{
		TypedFrame.SetSourcePosition(67, 6);
		([&]() -> int32
		{
			const int32 as_ast_e127_value = ([&]() -> int32
		{
			const int32 as_ast_e124_left = as_ast_d44;
			const int32 as_ast_e124_right = int32(1);
			return AngelscriptTypedASTJIT::LogicalShiftRight<int32>(as_ast_e124_left, as_ast_e124_right);
		}());
			int32& as_ast_e127_target = as_ast_d50;
			as_ast_e127_target = as_ast_e127_value;
			return as_ast_e127_target;
		}());
	}
	bool as_ast_d51;
	{
		TypedFrame.SetSourcePosition(69, 7);
		([&]() -> bool
		{
			const bool as_ast_e135_value = (as_ast_d42 && ([&]() -> bool
		{
			const int32 as_ast_e132_left = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d44);
			const int32 as_ast_e132_right = as_ast_d41;
			return (as_ast_e132_left > as_ast_e132_right);
		}()));
			bool& as_ast_e135_target = as_ast_d51;
			as_ast_e135_target = as_ast_e135_value;
			return as_ast_e135_target;
		}());
	}
	bool as_ast_d52;
	{
		TypedFrame.SetSourcePosition(70, 7);
		([&]() -> bool
		{
			const bool as_ast_e142_value = ((as_ast_d51 && as_ast_d43) && IsRunningCommandlet());
			bool& as_ast_e142_target = as_ast_d52;
			as_ast_e142_target = as_ast_e142_value;
			return as_ast_e142_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(72, 2);
		if (as_ast_d52)
		{
			{
				TypedFrame.SetSourcePosition(74, 3);
				return ([&]() -> int32
			{
				const int32 as_ast_e148_left = ([&]() -> int32
			{
				const int32 as_ast_e147_left = as_ast_d49;
				const int32 as_ast_e147_right = as_ast_d48;
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e147_left, as_ast_e147_right);
			}());
				const int32 as_ast_e148_right = as_ast_d44;
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e148_left, as_ast_e148_right);
			}());
			}
		}
	}
	{
		TypedFrame.SetSourcePosition(76, 2);
		if (([&]() -> bool
		{
			const int32 as_ast_e151_left = as_ast_d50;
			const int32 as_ast_e151_right = int32(10);
			return (as_ast_e151_left > as_ast_e151_right);
		}()))
		{
			{
				TypedFrame.SetSourcePosition(78, 3);
				return ([&]() -> int32
			{
				const int32 as_ast_e156_left = ([&]() -> int32
			{
				const int32 as_ast_e155_left = as_ast_d50;
				const int32 as_ast_e155_right = int32(2);
				return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e155_left, as_ast_e155_right);
			}());
				const int32 as_ast_e156_right = as_ast_d44;
				return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e156_left, as_ast_e156_right);
			}());
			}
		}
	}
	{
		TypedFrame.SetSourcePosition(80, 2);
		return ([&]() -> int32
	{
		const int32 as_ast_e164_left = ([&]() -> int32
	{
		const int32 as_ast_e163_left = as_ast_d47;
		const int32 as_ast_e163_right = int32(1);
		return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e163_left, as_ast_e163_right);
	}());
		const int32 as_ast_e164_right = as_ast_d44;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e164_left, as_ast_e164_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int TypedASTCapabilityShowcase(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:5
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:5
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
// AS Source   : /Angelscript/Game/ASStaticJITAotFixture.as:50:5
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
