// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Module
 * VirtualSourcePath   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as
 * CanonicalModuleName : ASStaticJITTypedControlFlowFixture
 * TargetProfile       : EditorDevelopment
 * StableModuleKey     : e3b08a3c509f1eb5fad8ca63479dcc6c2638c01694af2a1048e06e1de36d5c99
 * ProviderId          : 7dc70fd55a76cc55d726e32d5c2434713bd93755a32e4dd524a4b7c39c8b5d44
 * ArtifactProfile     : 018cc71ab6612eca7608aa13abe1f7a1a7d81964bb8cc5821f1b1e047fcf7a11
 * NativeEnvironment   : 76d620ec7d0db38aa2028996e595f75523f201e7e560c05d8c9000d60d04cd73
 * FunctionCount       : 4
 */

#if WITH_EDITOR && UE_BUILD_DEVELOPMENT
#include "StaticJIT/StaticJITConfig.h"
#ifndef AS_SKIP_JITTED_CODE
#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "StaticJIT/NativeCalls/AngelscriptStaticJITNativeCallables.h"
#include "StaticJIT/StaticJITHeader.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"
constexpr SIZE_T POFFSET_UStaticJITAotFunctionCarrier_StoredValue = Align(sizeof(UObject) + 0, 4);
constexpr SIZE_T PALIGN_UStaticJITAotFunctionCarrier_StoredValue = AlignmentMax(alignof(UObject), 4);
constexpr SIZE_T TALIGN_UStaticJITAotFunctionCarrier = PALIGN_UStaticJITAotFunctionCarrier_StoredValue;
constexpr SIZE_T TSIZE_UStaticJITAotFunctionCarrier = Align(POFFSET_UStaticJITAotFunctionCarrier_StoredValue + 4, TALIGN_UStaticJITAotFunctionCarrier);
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticExhaustiveEnum(const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
 * StableFunctionKey  : 0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4
 * ExecutionHash      : 4510fb38109260be32bcfce50e86695d28f31269bf605fdb6e474f5cf57480a5
 * DebugHash          : 56fda596b4afaff9e3abd0815538ef23617c4a0b90467947532d2fcfda90cde7
 * EntryAbiHash       : ac5e9740aa5289dba47d6e5ba3f40c6f1f11e575d2ab2615b198bbbc11568b3d
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_4510fb38109260be32bcfce50e86695d28f31269bf605fdb6e474f5cf57480a5_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d16);

static int32 ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_4510fb38109260be32bcfce50e86695d28f31269bf605fdb6e474f5cf57480a5_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d16)
{
	int8 as_ast_d17;
	{
		TypedFrame.SetSourcePosition(103, 23);
		([&]() -> int8
		{
			const int8 as_ast_e109_value = AngelscriptTypedASTJIT::WrapNarrow<int8>(as_ast_d16);
			int8& as_ast_e109_target = as_ast_d17;
			as_ast_e109_target = as_ast_e109_value;
			return as_ast_e109_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(104, 2);
		const auto as_ast_stmt_84_condition = as_ast_d17;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
		switch (as_ast_stmt_84_condition)
		{
			case static_cast<int8>(int8(0)):
				{
					TypedFrame.SetSourcePosition(107, 3);
					return int32(10);
				}
			case static_cast<int8>(int8(1)):
				{
					TypedFrame.SetSourcePosition(109, 3);
					return int32(20);
				}
			case static_cast<int8>(int8(2)):
				{
					TypedFrame.SetSourcePosition(111, 3);
					return int32(30);
				}
			default:
				TypedFrame.SetSourcePosition(111, 3);
				FStaticJITFunction::SetSwitchValueInvalidException(Execution);
				return {};
		}
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int SemanticExhaustiveEnum(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
// JIT Entry   : Raw
int32 ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_4510fb38109260be32bcfce50e86695d28f31269bf605fdb6e474f5cf57480a5_Raw(FScriptExecution& Execution, asDWORD p_0)
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
	return ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_4510fb38109260be32bcfce50e86695d28f31269bf605fdb6e474f5cf57480a5_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0));
}

// AS Function : int SemanticExhaustiveEnum(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
// JIT Entry   : VM
void ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_4510fb38109260be32bcfce50e86695d28f31269bf605fdb6e474f5cf57480a5_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_4510fb38109260be32bcfce50e86695d28f31269bf605fdb6e474f5cf57480a5_Raw(Execution,
		*(asDWORD*)(l_fp + 0));
}

// AS Function : int SemanticExhaustiveEnum(const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:101:1
// JIT Entry   : Parms
void ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_4510fb38109260be32bcfce50e86695d28f31269bf605fdb6e474f5cf57480a5_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_0145fa4429b9ae62be79d15799167aacc5b06d72777cceecf717a910ebec9dd4_4510fb38109260be32bcfce50e86695d28f31269bf605fdb6e474f5cf57480a5_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticCloneableState(const int, const int, const bool, const bool)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:1
 * StableFunctionKey  : 169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855
 * ExecutionHash      : cdf1bed19653804c1babbecf54b1b7577028a5d293ff606d55138e25b4670a63
 * DebugHash          : 1554eb32a7c6b4e41e92683e2a1877e834fe5249471d2f42e5ffa79209a8e712
 * EntryAbiHash       : 3d05f1b7e35a8891df3c73e1ce6d43c897b0603db3baca8a2f98651f018854ce
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 32
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_cdf1bed19653804c1babbecf54b1b7577028a5d293ff606d55138e25b4670a63_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d22,
	const int32 as_ast_d23,
	const bool as_ast_d24,
	const bool as_ast_d25);

static int32 ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_cdf1bed19653804c1babbecf54b1b7577028a5d293ff606d55138e25b4670a63_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d22,
	const int32 as_ast_d23,
	const bool as_ast_d24,
	const bool as_ast_d25)
{
	int32 as_ast_d26;
	{
		TypedFrame.SetSourcePosition(128, 6);
		([&]() -> int32
		{
			const int32 as_ast_e125_value = as_ast_d22;
			int32& as_ast_e125_target = as_ast_d26;
			as_ast_e125_target = as_ast_e125_value;
			return as_ast_e125_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(129, 2);
		switch (as_ast_d23)
		{
			case static_cast<int32>(int32(0)):
				{
					int32 as_ast_d27;
					{
						TypedFrame.SetSourcePosition(133, 7);
						([&]() -> int32
						{
							const int32 as_ast_e144_value = ([&]() -> int32
						{
							const int32 as_ast_e142_left = ([&]() -> int32
						{
							const int32 as_ast_e141_left = ([&]() -> int32
						{
							const int32 as_ast_e139_left = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d26);
							const int32 as_ast_e139_right = int32(100);
							return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e139_left, as_ast_e139_right);
						}());
							const int32 as_ast_e141_right = ([&]() -> int32
						{
							const int32 as_ast_e140_left = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d26);
							const int32 as_ast_e140_right = int32(10);
							return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e140_left, as_ast_e140_right);
						}());
							return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e141_left, as_ast_e141_right);
						}());
							const int32 as_ast_e142_right = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d26);
							return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e142_left, as_ast_e142_right);
						}());
							int32& as_ast_e144_target = as_ast_d27;
							as_ast_e144_target = as_ast_e144_value;
							return as_ast_e144_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(137, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e149_left = ([&]() -> int32
					{
						const int32 as_ast_e148_left = as_ast_d26;
						const int32 as_ast_e148_right = int32(1000);
						return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e148_left, as_ast_e148_right);
					}());
						const int32 as_ast_e149_right = as_ast_d27;
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e149_left, as_ast_e149_right);
					}());
					}
				}
			case static_cast<int32>(int32(1)):
				{
					bool as_ast_d28;
					{
						TypedFrame.SetSourcePosition(141, 8);
						([&]() -> bool
						{
							const bool as_ast_e161_value = ((as_ast_d24 && ([&]() -> bool
						{
							const int32 as_ast_e156_left = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d26);
							const int32 as_ast_e156_right = as_ast_d22;
							return (as_ast_e156_left > as_ast_e156_right);
						}())) && as_ast_d25);
							bool& as_ast_e161_target = as_ast_d28;
							as_ast_e161_target = as_ast_e161_value;
							return as_ast_e161_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(143, 3);
						if (as_ast_d28)
						{
							{
								TypedFrame.SetSourcePosition(145, 4);
								return ([&]() -> int32
							{
								const int32 as_ast_e167_left = ([&]() -> int32
							{
								const int32 as_ast_e166_left = as_ast_d26;
								const int32 as_ast_e166_right = int32(10);
								return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e166_left, as_ast_e166_right);
							}());
								const int32 as_ast_e167_right = int32(1);
								return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e167_left, as_ast_e167_right);
							}());
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(147, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e170_left = as_ast_d26;
						const int32 as_ast_e170_right = int32(10);
						return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e170_left, as_ast_e170_right);
					}());
					}
				}
			case static_cast<int32>(int32(2)):
				{
					{
						TypedFrame.SetSourcePosition(151, 3);
						([&]() -> int32
						{
							const int32 as_ast_e174_value = int32(3);
							int32& as_ast_e174_target = as_ast_d26;
							as_ast_e174_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e174_target, as_ast_e174_value);
							return as_ast_e174_target;
						}());
					}
					int32 as_ast_d29;
					{
						TypedFrame.SetSourcePosition(152, 7);
						([&]() -> int32
						{
							const int32 as_ast_e179_value = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d26);
							int32& as_ast_e179_target = as_ast_d29;
							as_ast_e179_target = as_ast_e179_value;
							return as_ast_e179_target;
						}());
					}
					int32 as_ast_d30;
					{
						TypedFrame.SetSourcePosition(153, 7);
						([&]() -> int32
						{
							const int32 as_ast_e184_value = AngelscriptTypedASTJIT::WrapPostIncrement<int32>(as_ast_d26);
							int32& as_ast_e184_target = as_ast_d30;
							as_ast_e184_target = as_ast_e184_value;
							return as_ast_e184_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(154, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e193_left = ([&]() -> int32
					{
						const int32 as_ast_e192_left = ([&]() -> int32
					{
						const int32 as_ast_e190_left = as_ast_d26;
						const int32 as_ast_e190_right = int32(100);
						return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e190_left, as_ast_e190_right);
					}());
						const int32 as_ast_e192_right = ([&]() -> int32
					{
						const int32 as_ast_e191_left = as_ast_d29;
						const int32 as_ast_e191_right = int32(10);
						return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e191_left, as_ast_e191_right);
					}());
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e192_left, as_ast_e192_right);
					}());
						const int32 as_ast_e193_right = as_ast_d30;
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e193_left, as_ast_e193_right);
					}());
					}
				}
			case static_cast<int32>(int32(3)):
				{
					int32 as_ast_d31;
					{
						TypedFrame.SetSourcePosition(158, 7);
						([&]() -> int32
						{
							const int32 as_ast_e197_value = int32(0);
							int32& as_ast_e197_target = as_ast_d31;
							as_ast_e197_target = as_ast_e197_value;
							return as_ast_e197_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(159, 3);
						while (([&]() -> bool
						{
							const int32 as_ast_e200_left = as_ast_d31;
							const int32 as_ast_e200_right = int32(3);
							return (as_ast_e200_left < as_ast_e200_right);
						}()))
						{
							{
								TypedFrame.SetSourcePosition(161, 4);
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d31);
							}
							{
								TypedFrame.SetSourcePosition(162, 4);
								if (([&]() -> bool
								{
									const int32 as_ast_e206_left = as_ast_d31;
									const int32 as_ast_e206_right = int32(1);
									return (as_ast_e206_left == as_ast_e206_right);
								}()))
								{
									{
										TypedFrame.SetSourcePosition(164, 5);
										continue;
									}
								}
							}
							{
								TypedFrame.SetSourcePosition(166, 4);
								([&]() -> int32
								{
									const int32 as_ast_e209_value = as_ast_d31;
									int32& as_ast_e209_target = as_ast_d26;
									as_ast_e209_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e209_target, as_ast_e209_value);
									return as_ast_e209_target;
								}());
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(169, 3);
						int32 as_ast_d32;
						{
							TypedFrame.SetSourcePosition(169, 12);
							([&]() -> int32
							{
								const int32 as_ast_e212_value = int32(0);
								int32& as_ast_e212_target = as_ast_d32;
								as_ast_e212_target = as_ast_e212_value;
								return as_ast_e212_target;
							}());
						}
						for ( ; ([&]() -> bool
					{
						const int32 as_ast_e215_left = as_ast_d32;
						const int32 as_ast_e215_right = int32(4);
						return (as_ast_e215_left < as_ast_e215_right);
					}()) ; AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d32) )
						{
							{
								TypedFrame.SetSourcePosition(171, 4);
								switch (as_ast_d32)
								{
									case static_cast<int32>(int32(1)):
										{
											TypedFrame.SetSourcePosition(174, 5);
											([&]() -> int32
											{
												const int32 as_ast_e223_value = int32(10);
												int32& as_ast_e223_target = as_ast_d26;
												as_ast_e223_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e223_target, as_ast_e223_value);
												return as_ast_e223_target;
											}());
										}
										{
											TypedFrame.SetSourcePosition(175, 5);
											continue;
										}
									case static_cast<int32>(int32(2)):
										{
											TypedFrame.SetSourcePosition(177, 5);
											([&]() -> int32
											{
												const int32 as_ast_e227_value = int32(20);
												int32& as_ast_e227_target = as_ast_d26;
												as_ast_e227_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e227_target, as_ast_e227_value);
												return as_ast_e227_target;
											}());
										}
										{
											TypedFrame.SetSourcePosition(178, 5);
											break;
										}
									default:
										{
											TypedFrame.SetSourcePosition(180, 5);
											([&]() -> int32
											{
												const int32 as_ast_e230_value = as_ast_d32;
												int32& as_ast_e230_target = as_ast_d26;
												as_ast_e230_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e230_target, as_ast_e230_value);
												return as_ast_e230_target;
											}());
										}
										{
											TypedFrame.SetSourcePosition(181, 5);
											break;
										}
								}
							}
							{
								TypedFrame.SetSourcePosition(184, 4);
								if (([&]() -> bool
								{
									const int32 as_ast_e235_left = as_ast_d32;
									const int32 as_ast_e235_right = int32(2);
									return (as_ast_e235_left == as_ast_e235_right);
								}()))
								{
									{
										TypedFrame.SetSourcePosition(186, 5);
										break;
									}
								}
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(189, 3);
						return as_ast_d26;
					}
				}
			case static_cast<int32>(int32(4)):
				{
					int32 as_ast_d33;
					{
						TypedFrame.SetSourcePosition(193, 7);
						([&]() -> int32
						{
							const int32 as_ast_e240_value = int32(0);
							int32& as_ast_e240_target = as_ast_d33;
							as_ast_e240_target = as_ast_e240_value;
							return as_ast_e240_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(194, 3);
						while (([&]() -> bool
						{
							const int32 as_ast_e247_left = ([&]() -> int32
						{
							TypedFrame.SetSourcePosition(194, 3);
							const int32 as_ast_e243_left = int32(10);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							const int32 as_ast_e243_right = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e243_left, as_ast_e243_right);
						}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e247_right = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d33);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e247_left > as_ast_e247_right);
						}()))
						{
							{
								TypedFrame.SetSourcePosition(196, 4);
								([&]() -> int32
								{
									const int32 as_ast_e250_value = int32(1);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e250_target = as_ast_d26;
									as_ast_e250_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e250_target, as_ast_e250_value);
									return as_ast_e250_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(197, 4);
								break;
							}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(199, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e253_left = as_ast_d26;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						const int32 as_ast_e253_right = as_ast_d33;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e253_left, as_ast_e253_right);
					}());
					}
				}
			case static_cast<int32>(int32(5)):
				{
					{
						TypedFrame.SetSourcePosition(203, 3);
						int32 as_ast_d34;
						{
							TypedFrame.SetSourcePosition(203, 12);
							([&]() -> int32
							{
								const int32 as_ast_e257_value = int32(0);
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return int32{};
								}
								int32& as_ast_e257_target = as_ast_d34;
								as_ast_e257_target = as_ast_e257_value;
								return as_ast_e257_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return {};
							}
						}
						for ( ; ([&]() -> bool
					{
						const int32 as_ast_e260_left = as_ast_d34;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						const int32 as_ast_e260_right = int32(1);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						return (as_ast_e260_left < as_ast_e260_right);
					}()) ; AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d34) )
						{
							{
								TypedFrame.SetSourcePosition(205, 4);
								([&]() -> int32
								{
									const int32 as_ast_e268_value = ([&]() -> int32
								{
									TypedFrame.SetSourcePosition(205, 4);
									const int32 as_ast_e267_left = int32(10);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									const int32 as_ast_e267_right = as_ast_d22;
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e267_left, as_ast_e267_right);
								}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e268_target = as_ast_d26;
									as_ast_e268_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e268_target, as_ast_e268_value);
									return as_ast_e268_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(206, 4);
								([&]() -> int32
								{
									const int32 as_ast_e271_value = int32(100);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e271_target = as_ast_d26;
									as_ast_e271_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e271_target, as_ast_e271_value);
									return as_ast_e271_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(208, 3);
						return as_ast_d26;
					}
				}
			case static_cast<int32>(int32(6)):
				{
					int32 as_ast_d35;
					{
						TypedFrame.SetSourcePosition(212, 7);
						([&]() -> int32
						{
							const int32 as_ast_e276_value = int32(0);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							int32& as_ast_e276_target = as_ast_d35;
							as_ast_e276_target = as_ast_e276_value;
							return as_ast_e276_target;
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(213, 3);
						int32 as_ast_d36;
						{
							TypedFrame.SetSourcePosition(213, 12);
							([&]() -> int32
							{
								const int32 as_ast_e279_value = int32(0);
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return int32{};
								}
								int32& as_ast_e279_target = as_ast_d36;
								as_ast_e279_target = as_ast_e279_value;
								return as_ast_e279_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return {};
							}
						}
						for ( ; ([&]() -> bool
					{
						const int32 as_ast_e282_left = as_ast_d36;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						const int32 as_ast_e282_right = int32(1);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						return (as_ast_e282_left < as_ast_e282_right);
					}()) ; (([&]() -> int32
						{
							const int32 as_ast_e287_value = ([&]() -> int32
						{
							TypedFrame.SetSourcePosition(213, 3);
							const int32 as_ast_e286_left = int32(10);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							const int32 as_ast_e286_right = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e286_left, as_ast_e286_right);
						}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							int32& as_ast_e287_target = as_ast_d36;
							as_ast_e287_target = as_ast_e287_value;
							return as_ast_e287_target;
						}()), AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d35)) )
						{
							{
								TypedFrame.SetSourcePosition(216, 4);
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d36);
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(217, 4);
								continue;
							}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(219, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e297_left = as_ast_d26;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						const int32 as_ast_e297_right = as_ast_d35;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e297_left, as_ast_e297_right);
					}());
					}
				}
			case static_cast<int32>(int32(7)):
				{
					{
						TypedFrame.SetSourcePosition(223, 3);
						const auto as_ast_stmt_190_condition = ([&]() -> int32
						{
							TypedFrame.SetSourcePosition(223, 3);
							const int32 as_ast_e301_left = int32(10);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							const int32 as_ast_e301_right = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e301_left, as_ast_e301_right);
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
						switch (as_ast_stmt_190_condition)
						{
							case static_cast<int32>(int32(0)):
								{
									TypedFrame.SetSourcePosition(226, 4);
									([&]() -> int32
									{
										const int32 as_ast_e305_value = int32(1);
										if (UNLIKELY(Execution.bExceptionThrown))
										{
											return int32{};
										}
										int32& as_ast_e305_target = as_ast_d26;
										as_ast_e305_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e305_target, as_ast_e305_value);
										return as_ast_e305_target;
									}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return {};
									}
								}
								{
									TypedFrame.SetSourcePosition(227, 4);
									break;
								}
							default:
								{
									TypedFrame.SetSourcePosition(229, 4);
									([&]() -> int32
									{
										const int32 as_ast_e308_value = int32(2);
										if (UNLIKELY(Execution.bExceptionThrown))
										{
											return int32{};
										}
										int32& as_ast_e308_target = as_ast_d26;
										as_ast_e308_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e308_target, as_ast_e308_value);
										return as_ast_e308_target;
									}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return {};
									}
								}
								{
									TypedFrame.SetSourcePosition(230, 4);
									break;
								}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(232, 3);
						return as_ast_d26;
					}
				}
			case static_cast<int32>(int32(8)):
				{
					int32 as_ast_d37;
					{
						TypedFrame.SetSourcePosition(236, 7);
						([&]() -> int32
						{
							const int32 as_ast_e314_value = int32(0);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							int32& as_ast_e314_target = as_ast_d37;
							as_ast_e314_target = as_ast_e314_value;
							return as_ast_e314_target;
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(237, 3);
						do
						{
							{
								TypedFrame.SetSourcePosition(239, 4);
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d37);
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(240, 4);
								continue;
							}
						}
						while (([&]() -> bool
						{
							const int32 as_ast_e322_left = ([&]() -> int32
						{
							TypedFrame.SetSourcePosition(237, 3);
							const int32 as_ast_e320_left = int32(10);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							const int32 as_ast_e320_right = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e320_left, as_ast_e320_right);
						}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e322_right = as_ast_d37;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e322_left > as_ast_e322_right);
						}()));
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(243, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e325_left = as_ast_d26;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						const int32 as_ast_e325_right = as_ast_d37;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e325_left, as_ast_e325_right);
					}());
					}
				}
			case static_cast<int32>(int32(10)):
				{
					{
						TypedFrame.SetSourcePosition(247, 3);
						do
						{
							{
								TypedFrame.SetSourcePosition(249, 4);
								([&]() -> int32
								{
									const int32 as_ast_e331_value = ([&]() -> int32
								{
									TypedFrame.SetSourcePosition(249, 4);
									const int32 as_ast_e330_left = int32(10);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									const int32 as_ast_e330_right = as_ast_d22;
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e330_left, as_ast_e330_right);
								}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e331_target = as_ast_d26;
									as_ast_e331_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e331_target, as_ast_e331_value);
									return as_ast_e331_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(250, 4);
								([&]() -> int32
								{
									const int32 as_ast_e334_value = int32(100);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e334_target = as_ast_d26;
									as_ast_e334_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e334_target, as_ast_e334_value);
									return as_ast_e334_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
						}
						while (AngelscriptTypedASTJIT::FromCanonicalBits<bool>(UINT64_C(0)));
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(253, 3);
						return as_ast_d26;
					}
				}
			case static_cast<int32>(int32(11)):
				{
					int32 as_ast_d38;
					{
						TypedFrame.SetSourcePosition(257, 7);
						([&]() -> int32
						{
							const int32 as_ast_e340_value = int32(0);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							int32& as_ast_e340_target = as_ast_d38;
							as_ast_e340_target = as_ast_e340_value;
							return as_ast_e340_target;
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(258, 3);
						while ((([&]() -> bool
						{
							const int32 as_ast_e347_left = as_ast_d38;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e347_right = int32(0);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e347_left == as_ast_e347_right);
						}()) || ([&]() -> bool
						{
							const int32 as_ast_e348_left = ([&]() -> int32
						{
							TypedFrame.SetSourcePosition(258, 3);
							const int32 as_ast_e345_left = int32(10);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							const int32 as_ast_e345_right = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e345_left, as_ast_e345_right);
						}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e348_right = as_ast_d38;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e348_left > as_ast_e348_right);
						}())))
						{
							{
								TypedFrame.SetSourcePosition(260, 4);
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d38);
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(261, 4);
								continue;
							}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(263, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e355_left = as_ast_d26;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						const int32 as_ast_e355_right = as_ast_d38;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e355_left, as_ast_e355_right);
					}());
					}
				}
			case static_cast<int32>(int32(12)):
				{
					{
						TypedFrame.SetSourcePosition(267, 3);
						int32 as_ast_d39;
						{
							TypedFrame.SetSourcePosition(267, 12);
							([&]() -> int32
							{
								const int32 as_ast_e359_value = int32(0);
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return int32{};
								}
								int32& as_ast_e359_target = as_ast_d39;
								as_ast_e359_target = as_ast_e359_value;
								return as_ast_e359_target;
							}());
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return {};
							}
						}
						for ( ; ([&]() -> bool
					{
						const int32 as_ast_e364_left = ([&]() -> int32
					{
						TypedFrame.SetSourcePosition(267, 3);
						const int32 as_ast_e362_left = int32(10);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						const int32 as_ast_e362_right = as_ast_d22;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e362_left, as_ast_e362_right);
					}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						const int32 as_ast_e364_right = as_ast_d39;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return bool{};
						}
						return (as_ast_e364_left > as_ast_e364_right);
					}()) ; AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d39) )
						{
							{
								TypedFrame.SetSourcePosition(269, 4);
								break;
							}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(271, 3);
						return as_ast_d26;
					}
				}
			case static_cast<int32>(int32(13)):
				{
					{
						TypedFrame.SetSourcePosition(275, 3);
						while (([&]() -> bool
						{
							const int32 as_ast_e372_left = as_ast_d26;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e372_right = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e372_left == as_ast_e372_right);
						}()))
						{
							{
								TypedFrame.SetSourcePosition(277, 4);
								([&]() -> int32
								{
									const int32 as_ast_e377_value = ([&]() -> int32
								{
									TypedFrame.SetSourcePosition(277, 4);
									const int32 as_ast_e376_left = int32(10);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									const int32 as_ast_e376_right = as_ast_d22;
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e376_left, as_ast_e376_right);
								}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e377_target = as_ast_d26;
									as_ast_e377_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e377_target, as_ast_e377_value);
									return as_ast_e377_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(278, 4);
								([&]() -> int32
								{
									const int32 as_ast_e380_value = int32(100);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e380_target = as_ast_d26;
									as_ast_e380_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e380_target, as_ast_e380_value);
									return as_ast_e380_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(279, 4);
								break;
							}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(281, 3);
						return as_ast_d26;
					}
				}
			case static_cast<int32>(int32(14)):
				{
					int32 as_ast_d40;
					{
						TypedFrame.SetSourcePosition(285, 7);
						([&]() -> int32
						{
							const int32 as_ast_e385_value = int32(0);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return int32{};
							}
							int32& as_ast_e385_target = as_ast_d40;
							as_ast_e385_target = as_ast_e385_value;
							return as_ast_e385_target;
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(286, 3);
						do
						{
							{
								TypedFrame.SetSourcePosition(288, 4);
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d40);
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
							{
								TypedFrame.SetSourcePosition(289, 4);
								const bool as_ast_stmt_252_condition = ([&]() -> bool
								{
									const int32 as_ast_e391_left = as_ast_d40;
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return bool{};
									}
									const int32 as_ast_e391_right = int32(1);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return bool{};
									}
									return (as_ast_e391_left == as_ast_e391_right);
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
								if (as_ast_stmt_252_condition)
								{
									{
										TypedFrame.SetSourcePosition(291, 5);
										continue;
									}
								}
							}
							{
								TypedFrame.SetSourcePosition(293, 4);
								([&]() -> int32
								{
									const int32 as_ast_e396_value = ([&]() -> int32
								{
									const int32 as_ast_e395_left = as_ast_d40;
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									const int32 as_ast_e395_right = int32(10);
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e395_left, as_ast_e395_right);
								}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return int32{};
									}
									int32& as_ast_e396_target = as_ast_d26;
									as_ast_e396_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e396_target, as_ast_e396_value);
									return as_ast_e396_target;
								}());
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
							}
						}
						while (([&]() -> bool
						{
							const int32 as_ast_e399_left = as_ast_d40;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e399_right = int32(3);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e399_left < as_ast_e399_right);
						}()));
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(296, 3);
						return ([&]() -> int32
					{
						const int32 as_ast_e404_left = ([&]() -> int32
					{
						const int32 as_ast_e403_left = as_ast_d26;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						const int32 as_ast_e403_right = int32(10);
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e403_left, as_ast_e403_right);
					}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						const int32 as_ast_e404_right = as_ast_d40;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return int32{};
						}
						return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e404_left, as_ast_e404_right);
					}());
					}
				}
			case static_cast<int32>(int32(15)):
				{
					{
						TypedFrame.SetSourcePosition(300, 3);
						const bool as_ast_stmt_260_condition = ([&]() -> bool
						{
							const int32 as_ast_e408_left = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e408_right = int32(0);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e408_left < as_ast_e408_right);
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
						if (as_ast_stmt_260_condition)
						{
							{
								TypedFrame.SetSourcePosition(302, 4);
								return AngelscriptTypedASTJIT::WrapNegate<int32>(int32(10));
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(304, 3);
						const bool as_ast_stmt_263_condition = ([&]() -> bool
						{
							const int32 as_ast_e413_left = as_ast_d22;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							const int32 as_ast_e413_right = int32(0);
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return bool{};
							}
							return (as_ast_e413_left == as_ast_e413_right);
						}());
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
						if (as_ast_stmt_263_condition)
						{
							{
								TypedFrame.SetSourcePosition(306, 4);
								return int32(0);
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(308, 3);
						const bool as_ast_stmt_272_condition = as_ast_d24;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
						if (as_ast_stmt_272_condition)
						{
							{
								TypedFrame.SetSourcePosition(310, 4);
								const bool as_ast_stmt_266_condition = as_ast_d25;
								if (UNLIKELY(Execution.bExceptionThrown))
								{
									return {};
								}
								if (as_ast_stmt_266_condition)
								{
									{
										TypedFrame.SetSourcePosition(312, 5);
										return int32(11);
									}
								}
							}
							{
								TypedFrame.SetSourcePosition(314, 4);
								return int32(12);
							}
						}
						else
						{
							TypedFrame.SetSourcePosition(316, 8);
							const bool as_ast_stmt_271_condition = as_ast_d25;
							if (UNLIKELY(Execution.bExceptionThrown))
							{
								return {};
							}
							if (as_ast_stmt_271_condition)
							{
								{
									TypedFrame.SetSourcePosition(318, 4);
									return int32(13);
								}
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(320, 3);
						return int32(14);
					}
				}
			case static_cast<int32>(int32(16)):
				{
					{
						TypedFrame.SetSourcePosition(324, 3);
						const auto as_ast_stmt_276_condition = as_ast_d22;
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
						switch (as_ast_stmt_276_condition)
						{
							case static_cast<int32>(int32(0)):
								{
									TypedFrame.SetSourcePosition(327, 4);
									([&]() -> int32
									{
										const int32 as_ast_e427_value = int32(1);
										if (UNLIKELY(Execution.bExceptionThrown))
										{
											return int32{};
										}
										int32& as_ast_e427_target = as_ast_d26;
										as_ast_e427_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e427_target, as_ast_e427_value);
										return as_ast_e427_target;
									}());
									if (UNLIKELY(Execution.bExceptionThrown))
									{
										return {};
									}
								}
							case static_cast<int32>(int32(1)):
								{
									int32 as_ast_d41;
									{
										TypedFrame.SetSourcePosition(331, 8);
										([&]() -> int32
										{
											const int32 as_ast_e431_value = int32(20);
											if (UNLIKELY(Execution.bExceptionThrown))
											{
												return int32{};
											}
											int32& as_ast_e431_target = as_ast_d41;
											as_ast_e431_target = as_ast_e431_value;
											return as_ast_e431_target;
										}());
										if (UNLIKELY(Execution.bExceptionThrown))
										{
											return {};
										}
									}
									{
										TypedFrame.SetSourcePosition(332, 4);
										([&]() -> int32
										{
											const int32 as_ast_e434_value = as_ast_d41;
											if (UNLIKELY(Execution.bExceptionThrown))
											{
												return int32{};
											}
											int32& as_ast_e434_target = as_ast_d26;
											as_ast_e434_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e434_target, as_ast_e434_value);
											return as_ast_e434_target;
										}());
										if (UNLIKELY(Execution.bExceptionThrown))
										{
											return {};
										}
									}
									{
										TypedFrame.SetSourcePosition(333, 4);
										break;
									}
								}
							default:
								{
									int32 as_ast_d42;
									{
										TypedFrame.SetSourcePosition(337, 8);
										([&]() -> int32
										{
											const int32 as_ast_e437_value = int32(30);
											if (UNLIKELY(Execution.bExceptionThrown))
											{
												return int32{};
											}
											int32& as_ast_e437_target = as_ast_d42;
											as_ast_e437_target = as_ast_e437_value;
											return as_ast_e437_target;
										}());
										if (UNLIKELY(Execution.bExceptionThrown))
										{
											return {};
										}
									}
									{
										TypedFrame.SetSourcePosition(338, 4);
										([&]() -> int32
										{
											const int32 as_ast_e440_value = as_ast_d42;
											if (UNLIKELY(Execution.bExceptionThrown))
											{
												return int32{};
											}
											int32& as_ast_e440_target = as_ast_d26;
											as_ast_e440_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e440_target, as_ast_e440_value);
											return as_ast_e440_target;
										}());
										if (UNLIKELY(Execution.bExceptionThrown))
										{
											return {};
										}
									}
									{
										TypedFrame.SetSourcePosition(339, 4);
										break;
									}
								}
						}
						if (UNLIKELY(Execution.bExceptionThrown))
						{
							return {};
						}
					}
					{
						TypedFrame.SetSourcePosition(342, 3);
						return as_ast_d26;
					}
				}
			default:
				{
					TypedFrame.SetSourcePosition(345, 3);
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

// AS Function : int SemanticCloneableState(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:1
// JIT Entry   : Raw
int32 ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_cdf1bed19653804c1babbecf54b1b7577028a5d293ff606d55138e25b4670a63_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1, asBYTE p_2, asBYTE p_3)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as",
		"int SemanticCloneableState(const int, const int, const bool, const bool)",
		122,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_cdf1bed19653804c1babbecf54b1b7577028a5d293ff606d55138e25b4670a63_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1), static_cast<bool>(p_2), static_cast<bool>(p_3));
}

// AS Function : int SemanticCloneableState(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:1
// JIT Entry   : VM
void ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_cdf1bed19653804c1babbecf54b1b7577028a5d293ff606d55138e25b4670a63_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_cdf1bed19653804c1babbecf54b1b7577028a5d293ff606d55138e25b4670a63_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1),
		*(asBYTE*)(l_fp + 2),
		*(asBYTE*)(l_fp + 3));
}

// AS Function : int SemanticCloneableState(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:1
// JIT Entry   : Parms
void ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_cdf1bed19653804c1babbecf54b1b7577028a5d293ff606d55138e25b4670a63_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_cdf1bed19653804c1babbecf54b1b7577028a5d293ff606d55138e25b4670a63_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)),
		static_cast<asBYTE>(*(bool*)(((SIZE_T)Parms) + ParmOffset_2)),
		static_cast<asBYTE>(*(bool*)(((SIZE_T)Parms) + ParmOffset_3)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticStructuredControlFlow(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:9:1
 * StableFunctionKey  : 5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43
 * ExecutionHash      : 4d1a712e9399d4e1ae0ed0dcd07f13af9ecbb6b40b7a1c994c12be91da20b17e
 * DebugHash          : 8e55f25da847ec7f00332a87cc7d3db21fe415344717fa18ce522554c520ebca
 * EntryAbiHash       : 2fe16db0121b593e09cfa3e040d4124228d3d043449cc2ba5580b68918773fef
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 32
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_4d1a712e9399d4e1ae0ed0dcd07f13af9ecbb6b40b7a1c994c12be91da20b17e_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d7,
	const int32 as_ast_d8);

static int32 ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_4d1a712e9399d4e1ae0ed0dcd07f13af9ecbb6b40b7a1c994c12be91da20b17e_TypedBody(
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d7,
	const int32 as_ast_d8)
{
	int32 as_ast_d9;
	{
		TypedFrame.SetSourcePosition(11, 6);
		([&]() -> int32
		{
			const int32 as_ast_e3_value = int32(0);
			int32& as_ast_e3_target = as_ast_d9;
			as_ast_e3_target = as_ast_e3_value;
			return as_ast_e3_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(12, 2);
		int32 as_ast_d10;
		{
			TypedFrame.SetSourcePosition(12, 11);
			([&]() -> int32
			{
				const int32 as_ast_e6_value = int32(0);
				int32& as_ast_e6_target = as_ast_d10;
				as_ast_e6_target = as_ast_e6_value;
				return as_ast_e6_target;
			}());
		}
		for ( ; ([&]() -> bool
	{
		const int32 as_ast_e9_left = as_ast_d10;
		const int32 as_ast_e9_right = as_ast_d7;
		return (as_ast_e9_left < as_ast_e9_right);
	}()) ; AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d10) )
		{
			{
				TypedFrame.SetSourcePosition(14, 3);
				if (([&]() -> bool
				{
					const int32 as_ast_e15_left = as_ast_d10;
					const int32 as_ast_e15_right = int32(1);
					return (as_ast_e15_left == as_ast_e15_right);
				}()))
				{
					{
						TypedFrame.SetSourcePosition(16, 4);
						continue;
					}
				}
			}
			{
				TypedFrame.SetSourcePosition(19, 3);
				switch (as_ast_d10)
				{
					case static_cast<int32>(int32(2)):
						{
							TypedFrame.SetSourcePosition(22, 4);
							([&]() -> int32
							{
								const int32 as_ast_e20_value = int32(20);
								int32& as_ast_e20_target = as_ast_d9;
								as_ast_e20_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e20_target, as_ast_e20_value);
								return as_ast_e20_target;
							}());
						}
						{
							TypedFrame.SetSourcePosition(23, 4);
							break;
						}
					case static_cast<int32>(int32(3)):
						{
							TypedFrame.SetSourcePosition(25, 4);
							([&]() -> int32
							{
								const int32 as_ast_e24_value = int32(30);
								int32& as_ast_e24_target = as_ast_d9;
								as_ast_e24_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e24_target, as_ast_e24_value);
								return as_ast_e24_target;
							}());
						}
						{
							TypedFrame.SetSourcePosition(26, 4);
							continue;
						}
					default:
						{
							TypedFrame.SetSourcePosition(28, 4);
							([&]() -> int32
							{
								const int32 as_ast_e27_value = as_ast_d10;
								int32& as_ast_e27_target = as_ast_d9;
								as_ast_e27_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e27_target, as_ast_e27_value);
								return as_ast_e27_target;
							}());
						}
						{
							TypedFrame.SetSourcePosition(29, 4);
							break;
						}
				}
			}
			{
				TypedFrame.SetSourcePosition(32, 3);
				if (([&]() -> bool
				{
					const int32 as_ast_e32_left = as_ast_d9;
					const int32 as_ast_e32_right = int32(45);
					return (as_ast_e32_left > as_ast_e32_right);
				}()))
				{
					{
						TypedFrame.SetSourcePosition(34, 4);
						break;
					}
				}
			}
		}
	}
	int32 as_ast_d11;
	{
		TypedFrame.SetSourcePosition(38, 6);
		([&]() -> int32
		{
			const int32 as_ast_e35_value = int32(0);
			int32& as_ast_e35_target = as_ast_d11;
			as_ast_e35_target = as_ast_e35_value;
			return as_ast_e35_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(39, 2);
		while (([&]() -> bool
		{
			const int32 as_ast_e38_left = as_ast_d11;
			const int32 as_ast_e38_right = as_ast_d7;
			return (as_ast_e38_left < as_ast_e38_right);
		}()))
		{
			{
				TypedFrame.SetSourcePosition(41, 3);
				AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d11);
			}
			{
				TypedFrame.SetSourcePosition(42, 3);
				if (([&]() -> bool
				{
					const int32 as_ast_e44_left = as_ast_d11;
					const int32 as_ast_e44_right = int32(2);
					return (as_ast_e44_left == as_ast_e44_right);
				}()))
				{
					{
						TypedFrame.SetSourcePosition(44, 4);
						continue;
					}
				}
			}
			{
				TypedFrame.SetSourcePosition(46, 3);
				([&]() -> int32
				{
					const int32 as_ast_e47_value = int32(3);
					int32& as_ast_e47_target = as_ast_d9;
					as_ast_e47_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e47_target, as_ast_e47_value);
					return as_ast_e47_target;
				}());
			}
			{
				TypedFrame.SetSourcePosition(47, 3);
				if (([&]() -> bool
				{
					const int32 as_ast_e50_left = as_ast_d11;
					const int32 as_ast_e50_right = int32(4);
					return (as_ast_e50_left >= as_ast_e50_right);
				}()))
				{
					{
						TypedFrame.SetSourcePosition(49, 4);
						break;
					}
				}
			}
		}
	}
	int32 as_ast_d12;
	{
		TypedFrame.SetSourcePosition(53, 6);
		([&]() -> int32
		{
			const int32 as_ast_e53_value = int32(0);
			int32& as_ast_e53_target = as_ast_d12;
			as_ast_e53_target = as_ast_e53_value;
			return as_ast_e53_target;
		}());
	}
	{
		TypedFrame.SetSourcePosition(54, 2);
		do
		{
			{
				TypedFrame.SetSourcePosition(56, 3);
				AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d12);
			}
			{
				TypedFrame.SetSourcePosition(57, 3);
				if (([&]() -> bool
				{
					const int32 as_ast_e59_left = as_ast_d12;
					const int32 as_ast_e59_right = int32(1);
					return (as_ast_e59_left == as_ast_e59_right);
				}()))
				{
					{
						TypedFrame.SetSourcePosition(59, 4);
						continue;
					}
				}
			}
			{
				TypedFrame.SetSourcePosition(61, 3);
				([&]() -> int32
				{
					const int32 as_ast_e62_value = int32(5);
					int32& as_ast_e62_target = as_ast_d9;
					as_ast_e62_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e62_target, as_ast_e62_value);
					return as_ast_e62_target;
				}());
			}
		}
		while (([&]() -> bool
		{
			const int32 as_ast_e65_left = as_ast_d12;
			const int32 as_ast_e65_right = int32(2);
			return (as_ast_e65_left < as_ast_e65_right);
		}()));
	}
	{
		TypedFrame.SetSourcePosition(65, 2);
		switch (as_ast_d8)
		{
			case static_cast<int32>(int32(0)):
				{
					TypedFrame.SetSourcePosition(68, 3);
					([&]() -> int32
					{
						const int32 as_ast_e70_value = int32(100);
						int32& as_ast_e70_target = as_ast_d9;
						as_ast_e70_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e70_target, as_ast_e70_value);
						return as_ast_e70_target;
					}());
				}
				{
					TypedFrame.SetSourcePosition(69, 3);
					break;
				}
			case static_cast<int32>(int32(1)):
				{
					TypedFrame.SetSourcePosition(71, 3);
					([&]() -> int32
					{
						const int32 as_ast_e74_value = int32(200);
						int32& as_ast_e74_target = as_ast_d9;
						as_ast_e74_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e74_target, as_ast_e74_value);
						return as_ast_e74_target;
					}());
				}
			case static_cast<int32>(int32(2)):
				{
					int32 as_ast_d13;
					{
						TypedFrame.SetSourcePosition(74, 7);
						([&]() -> int32
						{
							const int32 as_ast_e78_value = int32(20);
							int32& as_ast_e78_target = as_ast_d13;
							as_ast_e78_target = as_ast_e78_value;
							return as_ast_e78_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(75, 3);
						([&]() -> int32
						{
							const int32 as_ast_e81_value = as_ast_d13;
							int32& as_ast_e81_target = as_ast_d9;
							as_ast_e81_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e81_target, as_ast_e81_value);
							return as_ast_e81_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(76, 3);
						break;
					}
				}
			case static_cast<int32>(int32(3)):
				{
					int32 as_ast_d14;
					{
						TypedFrame.SetSourcePosition(80, 7);
						([&]() -> int32
						{
							const int32 as_ast_e85_value = int32(0);
							int32& as_ast_e85_target = as_ast_d14;
							as_ast_e85_target = as_ast_e85_value;
							return as_ast_e85_target;
						}());
					}
					{
						TypedFrame.SetSourcePosition(81, 3);
						while (([&]() -> bool
						{
							const int32 as_ast_e88_left = as_ast_d14;
							const int32 as_ast_e88_right = int32(2);
							return (as_ast_e88_left < as_ast_e88_right);
						}()))
						{
							{
								TypedFrame.SetSourcePosition(83, 4);
								([&]() -> int32
								{
									const int32 as_ast_e91_value = int32(7);
									int32& as_ast_e91_target = as_ast_d9;
									as_ast_e91_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e91_target, as_ast_e91_value);
									return as_ast_e91_target;
								}());
							}
							{
								TypedFrame.SetSourcePosition(84, 4);
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d14);
							}
							{
								TypedFrame.SetSourcePosition(85, 4);
								if (([&]() -> bool
								{
									const int32 as_ast_e97_left = as_ast_d14;
									const int32 as_ast_e97_right = int32(1);
									return (as_ast_e97_left == as_ast_e97_right);
								}()))
								{
									{
										TypedFrame.SetSourcePosition(87, 5);
										continue;
									}
								}
							}
							{
								TypedFrame.SetSourcePosition(89, 4);
								break;
							}
						}
					}
					{
						TypedFrame.SetSourcePosition(91, 3);
						break;
					}
				}
			default:
				{
					TypedFrame.SetSourcePosition(94, 3);
					([&]() -> int32
					{
						const int32 as_ast_e100_value = int32(9);
						int32& as_ast_e100_target = as_ast_d9;
						as_ast_e100_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e100_target, as_ast_e100_value);
						return as_ast_e100_target;
					}());
				}
				{
					TypedFrame.SetSourcePosition(95, 3);
					break;
				}
		}
	}
	{
		TypedFrame.SetSourcePosition(97, 2);
		return as_ast_d9;
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int SemanticStructuredControlFlow(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:9:1
// JIT Entry   : Raw
int32 ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_4d1a712e9399d4e1ae0ed0dcd07f13af9ecbb6b40b7a1c994c12be91da20b17e_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
{
	SCRIPT_ASSUME_NO_EXCEPTION()
	FScopeTypedASTJITExecutionFrame TypedFrame(
		Execution,
		Execution.jitFunction,
		"/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as",
		"int SemanticStructuredControlFlow(const int, const int)",
		9,
		nullptr,
		true);
	if (UNLIKELY(!TypedFrame.IsActive()))
	{
		return {};
	}
	return ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_4d1a712e9399d4e1ae0ed0dcd07f13af9ecbb6b40b7a1c994c12be91da20b17e_TypedBody(TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int SemanticStructuredControlFlow(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:9:1
// JIT Entry   : VM
void ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_4d1a712e9399d4e1ae0ed0dcd07f13af9ecbb6b40b7a1c994c12be91da20b17e_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_4d1a712e9399d4e1ae0ed0dcd07f13af9ecbb6b40b7a1c994c12be91da20b17e_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int SemanticStructuredControlFlow(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:9:1
// JIT Entry   : Parms
void ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_4d1a712e9399d4e1ae0ed0dcd07f13af9ecbb6b40b7a1c994c12be91da20b17e_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_5371ead826b6cbce067d143d869aca985b0442c720e75dda0d4be73d528acb43_4d1a712e9399d4e1ae0ed0dcd07f13af9ecbb6b40b7a1c994c12be91da20b17e_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
/*
 * AngelScript Static JIT Function
 * Declaration       : int SemanticCheckedDivide(const int, const int)
 * Source             : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
 * StableFunctionKey  : a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5
 * ExecutionHash      : 2d6f94e8736fe05095956f3338d3e474d08496f05b81aceeda3f8357b3575a55
 * DebugHash          : 1c1aea33e7d9a3c50555638e8ca9006895853eecf51796f36e1416f1d9e4436f
 * EntryAbiHash       : 34574e76e092ff59a9ab137303c6c8a1a00f50e50bf9ce931ed07453463297ff
 * ExecutionCaps      : 129
 * RuntimeTriggers    : 0
 * CapabilityHash     : 5fff2a330081dc25acb31e4837294258d821f8533d45073e34a20c6aea6c3eb1
 */
static int32 ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_2d6f94e8736fe05095956f3338d3e474d08496f05b81aceeda3f8357b3575a55_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d19,
	const int32 as_ast_d20);

static int32 ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_2d6f94e8736fe05095956f3338d3e474d08496f05b81aceeda3f8357b3575a55_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d19,
	const int32 as_ast_d20)
{
	{
		TypedFrame.SetSourcePosition(118, 2);
		return ([&]() -> int32
	{
		TypedFrame.SetSourcePosition(118, 2);
		const int32 as_ast_e122_left = as_ast_d19;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		const int32 as_ast_e122_right = as_ast_d20;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return int32{};
		}
		return AngelscriptTypedASTJIT::CheckedDivide<int32>(Execution, as_ast_e122_left, as_ast_e122_right);
	}());
	}
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

// AS Function : int SemanticCheckedDivide(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
// JIT Entry   : Raw
int32 ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_2d6f94e8736fe05095956f3338d3e474d08496f05b81aceeda3f8357b3575a55_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1)
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
	return ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_2d6f94e8736fe05095956f3338d3e474d08496f05b81aceeda3f8357b3575a55_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1));
}

// AS Function : int SemanticCheckedDivide(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
// JIT Entry   : VM
void ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_2d6f94e8736fe05095956f3338d3e474d08496f05b81aceeda3f8357b3575a55_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_2d6f94e8736fe05095956f3338d3e474d08496f05b81aceeda3f8357b3575a55_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1));
}

// AS Function : int SemanticCheckedDivide(const int, const int)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:116:1
// JIT Entry   : Parms
void ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_2d6f94e8736fe05095956f3338d3e474d08496f05b81aceeda3f8357b3575a55_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = ASJIT_a17ec19a98294d0303cc3ef87a4c4c4c3966667c2d6e59668fe556b0282624d5_2d6f94e8736fe05095956f3338d3e474d08496f05b81aceeda3f8357b3575a55_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)));
}
#endif
#endif // WITH_EDITOR && UE_BUILD_DEVELOPMENT
