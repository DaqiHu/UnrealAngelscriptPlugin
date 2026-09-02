// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript Static JIT Isolated Differential Artifact
 * Backend            : TypedASTJIT
 * Declaration        : int SemanticCloneableState(const int, const int, const bool, const bool)
 * StableFunctionKey  : 169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855
 * OriginalSymbolSet  : ASJIT_169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855_cdf1bed19653804c1babbecf54b1b7577028a5d293ff606d55138e25b4670a63
 * TestSymbolSet      : AS_Test_Differential_TypedAST_SemanticCloneableState
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
static int32 AS_Test_Differential_TypedAST_SemanticCloneableState_TypedBody(
	FScriptExecution& Execution,
	FScopeTypedASTJITExecutionFrame& TypedFrame,
	const int32 as_ast_d22,
	const int32 as_ast_d23,
	const bool as_ast_d24,
	const bool as_ast_d25);

static int32 AS_Test_Differential_TypedAST_SemanticCloneableState_TypedBody(
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
int32 AS_Test_Differential_TypedAST_SemanticCloneableState_Raw(FScriptExecution& Execution, asDWORD p_0, asDWORD p_1, asBYTE p_2, asBYTE p_3)
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
	return AS_Test_Differential_TypedAST_SemanticCloneableState_TypedBody(Execution, TypedFrame, static_cast<int32>(p_0), static_cast<int32>(p_1), static_cast<bool>(p_2), static_cast<bool>(p_3));
}

// AS Function : int SemanticCloneableState(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:1
// JIT Entry   : VM
void AS_Test_Differential_TypedAST_SemanticCloneableState_VM(FScriptExecution& Execution, asDWORD* l_fp, asQWORD* l_outValue)
{
	*(int32*)l_outValue = AS_Test_Differential_TypedAST_SemanticCloneableState_Raw(Execution,
		*(asDWORD*)(l_fp + 0),
		*(asDWORD*)(l_fp + 1),
		*(asBYTE*)(l_fp + 2),
		*(asBYTE*)(l_fp + 3));
}

// AS Function : int SemanticCloneableState(const int, const int, const bool, const bool)
// AS Source   : /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as:122:1
// JIT Entry   : Parms
void AS_Test_Differential_TypedAST_SemanticCloneableState_Parms(FScriptExecution& Execution, void* Object, void* Parms)
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
	*(int32*)(((SIZE_T)Parms) + ReturnParmOffset) = AS_Test_Differential_TypedAST_SemanticCloneableState_Raw(Execution,
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_0)),
		static_cast<asDWORD>(*(int32*)(((SIZE_T)Parms) + ParmOffset_1)),
		static_cast<asBYTE>(*(bool*)(((SIZE_T)Parms) + ParmOffset_2)),
		static_cast<asBYTE>(*(bool*)(((SIZE_T)Parms) + ParmOffset_3)));
}

namespace
{
	struct FTypedASTJITDifferentialEntryRegistration
	{
		FTypedASTJITDifferentialEntryRegistration()
		{
			AngelscriptTestJITProbes::RegisterIsolatedDifferentialEntry(
				AngelscriptTestJITProbes::EIsolatedDifferentialBackend::TypedASTJIT,
				&AS_Test_Differential_TypedAST_SemanticCloneableState_VM,
				reinterpret_cast<asJITFunction_Raw>(&AS_Test_Differential_TypedAST_SemanticCloneableState_Raw),
				&AS_Test_Differential_TypedAST_SemanticCloneableState_Parms,
				TEXT("AS_Test_Differential_TypedAST_SemanticCloneableState"),
				TEXT("169ac06f87b6ff4213bade0bb8d12b1d533553178b5660298da6a6e66fd90855"),
				TEXT("int SemanticCloneableState(const int, const int, const bool, const bool)"),
				TEXT("/Angelscript/Game/ASStaticJITTypedControlFlowFixture.as"),
				122u,
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
