// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : TypedASTJIT
 * Declaration        : int SemanticNumericBoundary(const int, const int, const int, const int)
 * StableFunctionKey  : de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e
 * OriginalSymbolSet  : ASJIT_de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e_e0fe8c2cfcfada2c40ea089f18c78e9feb710c6195f0711860b0af886352893c
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
 * Source             : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:6
 * StableFunctionKey  : de11fe956606b543fa19fba3bf01592edab537ccc7e1e46ed59d0b8e3bdc256e
 * ExecutionHash      : e0fe8c2cfcfada2c40ea089f18c78e9feb710c6195f0711860b0af886352893c
 * DebugHash          : 298092bc25472d52eb973f769b952fdeb6d5acb420383e9766f3aad735d13526
 * EntryAbiHash       : ce69b03c30f4aba54525c218c4551a91f6dfe1f289fefa74e6e6ea01b1bab739
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 AS_Test_Differential_TypedAST_SemanticNumericBoundary_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d17,
	const int32 as_ast_d18,
	const int32 as_ast_d19,
	const int32 as_ast_d20);

static int32 AS_Test_Differential_TypedAST_SemanticNumericBoundary_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d17,
	const int32 as_ast_d18,
	const int32 as_ast_d19,
	const int32 as_ast_d20)
{
	{
		TypedFrame.SetSourcePosition(32, 3);
		switch (as_ast_d20)
		{
			case int32(0):
				{
					TypedFrame.SetSourcePosition(35, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e16_left = as_ast_d17;
					const int32 as_ast_e16_right = as_ast_d18;
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e16_left, as_ast_e16_right);
				}());
				}
			case int32(1):
				{
					TypedFrame.SetSourcePosition(37, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e20_left = as_ast_d17;
					const int32 as_ast_e20_right = as_ast_d18;
					return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e20_left, as_ast_e20_right);
				}());
				}
			case int32(2):
				{
					TypedFrame.SetSourcePosition(39, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e24_left = as_ast_d17;
					const int32 as_ast_e24_right = as_ast_d18;
					return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e24_left, as_ast_e24_right);
				}());
				}
			case int32(3):
				{
					TypedFrame.SetSourcePosition(41, 4);
					return ([&]() -> int32
				{
					TypedFrame.SetSourcePosition(41, 4);
					const int32 as_ast_e28_left = as_ast_d17;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e28_right = as_ast_d18;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e28_left, as_ast_e28_right);
				}());
				}
			case int32(4):
				{
					TypedFrame.SetSourcePosition(43, 4);
					return ([&]() -> int32
				{
					TypedFrame.SetSourcePosition(43, 4);
					const int32 as_ast_e32_left = as_ast_d17;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e32_right = as_ast_d18;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::CheckedRemainder<int32>(Execution, as_ast_e32_left, as_ast_e32_right);
				}());
				}
			case int32(5):
				{
					TypedFrame.SetSourcePosition(45, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e36_left = as_ast_d17;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e36_right = as_ast_d19;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::ShiftLeft<int32>(as_ast_e36_left, as_ast_e36_right);
				}());
				}
			case int32(6):
				{
					TypedFrame.SetSourcePosition(47, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e40_left = as_ast_d17;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e40_right = as_ast_d19;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::LogicalShiftRight<int32>(as_ast_e40_left, as_ast_e40_right);
				}());
				}
			case int32(7):
				{
					TypedFrame.SetSourcePosition(49, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e44_left = as_ast_d17;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e44_right = as_ast_d19;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::ArithmeticShiftRight<int32>(as_ast_e44_left, as_ast_e44_right);
				}());
				}
			case int32(8):
				{
					int64 as_ast_d23;
					{
						TypedFrame.SetSourcePosition(52, 10);
						([&]() -> int64
						{
							const int64 as_ast_e49_value = static_cast<int64>(as_ast_d17);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int64{};
							}
							int64& as_ast_e49_target = as_ast_d23;
							as_ast_e49_target = as_ast_e49_value;
							return as_ast_e49_target;
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(53, 4);
						const bool as_ast_stmt_29_condition = ([&]() -> bool
						{
							const int64 as_ast_e53_left = as_ast_d23;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int64 as_ast_e53_right = static_cast<int64>(as_ast_d17);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e53_left == as_ast_e53_right);
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
						if (as_ast_stmt_29_condition)
						{
							{
								TypedFrame.SetSourcePosition(55, 5);
								return as_ast_d17;
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(57, 4);
						return int32(0);
					}
				}
			case int32(9):
				{
					TypedFrame.SetSourcePosition(60, 4);
					const bool as_ast_stmt_35_condition = ([&]() -> bool
					{
						const int32 as_ast_e59_left = as_ast_d17;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						const int32 as_ast_e59_right = int32(0);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						return (as_ast_e59_left != as_ast_e59_right);
					}());
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return {};
					}
					if (as_ast_stmt_35_condition)
					{
						{
							TypedFrame.SetSourcePosition(62, 5);
							return int32(1);
						}
					}
				}
				{
					TypedFrame.SetSourcePosition(64, 4);
					return int32(0);
				}
			case int32(10):
				{
					TypedFrame.SetSourcePosition(66, 4);
					return ([&]() -> int32
				{
					const int32 as_ast_e65_left = AngelscriptTypedASTJIT::FromCanonicalBits<int32>(UINT64_C(7001));
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					const int32 as_ast_e65_right = as_ast_d17;
					if (UNLIKELY(Execution.bExceptionThrown))
					{
						return int32{};
					}
					return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e65_left, as_ast_e65_right);
				}());
				}
			default:
				{
					TypedFrame.SetSourcePosition(68, 4);
					return AngelscriptTypedASTJIT::WrapNegate<int32>(int32(1));
				}
		}
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int SemanticNumericBoundary(const int, const int, const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:6
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
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:6
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
// AS Source   : /Angelscript/Game/ASStaticJITTypedFinalScalarFixture.as:26:6
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
				6u,
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
