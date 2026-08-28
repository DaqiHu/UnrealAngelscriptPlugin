// @angelscript-jit-owned revision=2 kind=module-source
/*
 * AngelScript TypedASTJIT Test Probe
 * Declarations:
 *   - int SemanticScalarBranch(int, int)
 *   - int SemanticMutationParity(int, int)
 *   - float32 SemanticPowerFloat(float32, float32)
 *   - float64 SemanticPowerDouble(float64, float64)
 *   - float64 SemanticPowerDoubleInt(float64, int)
 *   - int SemanticStructuredControlFlow(int, int)
 *   - int SemanticExhaustiveEnum(int)
 * Sources     : /Angelscript/Game/ASStaticJITAotFixture.as
 *               /Angelscript/Game/ASStaticJITTypedControlFlowFixture.as
 * Scope       : test-only; not a Provider entry
 */
#include "AngelscriptTestJITProbes.h"
#include "CoreMinimal.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"

namespace
{
static int32 AS_Test_Semantic_SemanticScalarBranch(
	const int32 as_ast_d7,
	const int32 as_ast_d8)
{
	int32 as_ast_d9;
	{
		([&]() -> int32
		{
			const int32 as_ast_e9_value = ([&]() -> int32
		{
			const int32 as_ast_e7_left = as_ast_d7;
			const int32 as_ast_e7_right = as_ast_d8;
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e7_left, as_ast_e7_right);
		}());
			int32& as_ast_e9_target = as_ast_d9;
			as_ast_e9_target = as_ast_e9_value;
			return as_ast_e9_target;
		}());
	}
	{
		if (([&]() -> bool
		{
			const int32 as_ast_e12_left = as_ast_d9;
			const int32 as_ast_e12_right = int32(10);
			return (as_ast_e12_left > as_ast_e12_right);
		}()))
		{
			return ([&]() -> int32
			{
				const int32 as_ast_e15_left = as_ast_d9;
				const int32 as_ast_e15_right = int32(2);
				return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e15_left, as_ast_e15_right);
			}());
		}
	}
	return ([&]() -> int32
	{
		const int32 as_ast_e21_left = as_ast_d9;
		const int32 as_ast_e21_right = int32(1);
		return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e21_left, as_ast_e21_right);
	}());
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

static int32 AS_Test_Semantic_SemanticMutationParity(
	const int32 as_ast_d14,
	const int32 as_ast_d15)
{
	int32 as_ast_d16;
	{
		([&]() -> int32
		{
			const int32 as_ast_e25_value = as_ast_d14;
			int32& as_ast_e25_target = as_ast_d16;
			as_ast_e25_target = as_ast_e25_value;
			return as_ast_e25_target;
		}());
	}
	{
		([&]() -> int32
		{
			const int32 as_ast_e28_value = as_ast_d15;
			int32& as_ast_e28_target = as_ast_d16;
			as_ast_e28_target = as_ast_e28_value;
			return as_ast_e28_target;
		}());
	}
	{
		([&]() -> int32
		{
			const int32 as_ast_e33_value = int32(3);
			int32& as_ast_e33_target = as_ast_d16;
			as_ast_e33_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e33_target, as_ast_e33_value);
			return as_ast_e33_target;
		}());
	}
	int32 as_ast_d17;
	{
		([&]() -> int32
		{
			const int32 as_ast_e39_value = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d16);
			int32& as_ast_e39_target = as_ast_d17;
			as_ast_e39_target = as_ast_e39_value;
			return as_ast_e39_target;
		}());
	}
	int32 as_ast_d18;
	{
		([&]() -> int32
		{
			const int32 as_ast_e44_value = (as_ast_d16, AngelscriptTypedASTJIT::WrapPostIncrement<int32>(as_ast_d16));
			int32& as_ast_e44_target = as_ast_d18;
			as_ast_e44_target = as_ast_e44_value;
			return as_ast_e44_target;
		}());
	}
	return ([&]() -> int32
	{
		const int32 as_ast_e53_left = ([&]() -> int32
	{
		const int32 as_ast_e52_left = ([&]() -> int32
	{
		const int32 as_ast_e49_left = as_ast_d16;
		const int32 as_ast_e49_right = int32(1000);
		return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e49_left, as_ast_e49_right);
	}());
		const int32 as_ast_e52_right = ([&]() -> int32
	{
		const int32 as_ast_e51_left = as_ast_d17;
		const int32 as_ast_e51_right = int32(100);
		return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e51_left, as_ast_e51_right);
	}());
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e52_left, as_ast_e52_right);
	}());
		const int32 as_ast_e53_right = as_ast_d18;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e53_left, as_ast_e53_right);
	}());
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

static float AS_Test_Semantic_SemanticPowerFloat(
	FScriptExecution& Execution,
	const float as_ast_d20,
	const float as_ast_d21)
{
	return ([&]() -> float
	{
		const float as_ast_e58_left = as_ast_d20;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return float{};
		}
		const float as_ast_e58_right = as_ast_d21;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return float{};
		}
		return AngelscriptTypedASTJIT::CheckedPower<float>(Execution, as_ast_e58_left, as_ast_e58_right);
	}());
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return float{};
}

static double AS_Test_Semantic_SemanticPowerDouble(
	FScriptExecution& Execution,
	const double as_ast_d23,
	const double as_ast_d24)
{
	return ([&]() -> double
	{
		const double as_ast_e61_left = as_ast_d23;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		const double as_ast_e61_right = as_ast_d24;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		return AngelscriptTypedASTJIT::CheckedPower<double>(Execution, as_ast_e61_left, as_ast_e61_right);
	}());
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return double{};
}

static double AS_Test_Semantic_SemanticPowerDoubleInt(
	FScriptExecution& Execution,
	const double as_ast_d26,
	const int32 as_ast_d27)
{
	return ([&]() -> double
	{
		const double as_ast_e65_left = as_ast_d26;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		const double as_ast_e65_right = static_cast<double>(as_ast_d27);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		return AngelscriptTypedASTJIT::CheckedPower<double>(Execution, as_ast_e65_left, as_ast_e65_right);
	}());
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return double{};
}

static int32 AS_Test_Semantic_SemanticStructuredControlFlow(
	const int32 as_ast_d7,
	const int32 as_ast_d8)
{
	int32 as_ast_d9;
	{
		([&]() -> int32
		{
			const int32 as_ast_e3_value = int32(0);
			int32& as_ast_e3_target = as_ast_d9;
			as_ast_e3_target = as_ast_e3_value;
			return as_ast_e3_target;
		}());
	}
	{
		int32 as_ast_d10;
		{
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
				if (([&]() -> bool
				{
					const int32 as_ast_e14_left = as_ast_d10;
					const int32 as_ast_e14_right = int32(1);
					return (as_ast_e14_left == as_ast_e14_right);
				}()))
				{
					continue;
				}
			}
			{
				switch (as_ast_d10)
				{
					case int32(2):
						{
							([&]() -> int32
							{
								const int32 as_ast_e22_value = int32(20);
								int32& as_ast_e22_target = as_ast_d9;
								as_ast_e22_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e22_target, as_ast_e22_value);
								return as_ast_e22_target;
							}());
						}
						break;
					case int32(3):
						{
							([&]() -> int32
							{
								const int32 as_ast_e30_value = int32(30);
								int32& as_ast_e30_target = as_ast_d9;
								as_ast_e30_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e30_target, as_ast_e30_value);
								return as_ast_e30_target;
							}());
						}
						continue;
					default:
						{
							([&]() -> int32
							{
								const int32 as_ast_e37_value = as_ast_d10;
								int32& as_ast_e37_target = as_ast_d9;
								as_ast_e37_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e37_target, as_ast_e37_value);
								return as_ast_e37_target;
							}());
						}
						break;
				}
			}
			{
				if (([&]() -> bool
				{
					const int32 as_ast_e41_left = as_ast_d9;
					const int32 as_ast_e41_right = int32(45);
					return (as_ast_e41_left > as_ast_e41_right);
				}()))
				{
					break;
				}
			}
		}
	}
	int32 as_ast_d11;
	{
		([&]() -> int32
		{
			const int32 as_ast_e46_value = int32(0);
			int32& as_ast_e46_target = as_ast_d11;
			as_ast_e46_target = as_ast_e46_value;
			return as_ast_e46_target;
		}());
	}
	{
		while (([&]() -> bool
		{
			const int32 as_ast_e49_left = as_ast_d11;
			const int32 as_ast_e49_right = as_ast_d7;
			return (as_ast_e49_left < as_ast_e49_right);
		}()))
		{
			{
				AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d11);
			}
			{
				if (([&]() -> bool
				{
					const int32 as_ast_e54_left = as_ast_d11;
					const int32 as_ast_e54_right = int32(2);
					return (as_ast_e54_left == as_ast_e54_right);
				}()))
				{
					continue;
				}
			}
			{
				([&]() -> int32
				{
					const int32 as_ast_e61_value = int32(3);
					int32& as_ast_e61_target = as_ast_d9;
					as_ast_e61_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e61_target, as_ast_e61_value);
					return as_ast_e61_target;
				}());
			}
			{
				if (([&]() -> bool
				{
					const int32 as_ast_e66_left = as_ast_d11;
					const int32 as_ast_e66_right = int32(4);
					return (as_ast_e66_left >= as_ast_e66_right);
				}()))
				{
					break;
				}
			}
		}
	}
	int32 as_ast_d12;
	{
		([&]() -> int32
		{
			const int32 as_ast_e71_value = int32(0);
			int32& as_ast_e71_target = as_ast_d12;
			as_ast_e71_target = as_ast_e71_value;
			return as_ast_e71_target;
		}());
	}
	{
		do
		{
			{
				AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d12);
			}
			{
				if (([&]() -> bool
				{
					const int32 as_ast_e76_left = as_ast_d12;
					const int32 as_ast_e76_right = int32(1);
					return (as_ast_e76_left == as_ast_e76_right);
				}()))
				{
					continue;
				}
			}
			{
				([&]() -> int32
				{
					const int32 as_ast_e83_value = int32(5);
					int32& as_ast_e83_target = as_ast_d9;
					as_ast_e83_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e83_target, as_ast_e83_value);
					return as_ast_e83_target;
				}());
			}
		}
		while (([&]() -> bool
		{
			const int32 as_ast_e88_left = as_ast_d12;
			const int32 as_ast_e88_right = int32(2);
			return (as_ast_e88_left < as_ast_e88_right);
		}()));
	}
	{
		switch (as_ast_d8)
		{
			case int32(0):
				{
					([&]() -> int32
					{
						const int32 as_ast_e96_value = int32(100);
						int32& as_ast_e96_target = as_ast_d9;
						as_ast_e96_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e96_target, as_ast_e96_value);
						return as_ast_e96_target;
					}());
				}
				break;
			case int32(1):
				{
					([&]() -> int32
					{
						const int32 as_ast_e104_value = int32(200);
						int32& as_ast_e104_target = as_ast_d9;
						as_ast_e104_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e104_target, as_ast_e104_value);
						return as_ast_e104_target;
					}());
				}
			case int32(2):
				{
					int32 as_ast_d13;
					{
						([&]() -> int32
						{
							const int32 as_ast_e110_value = int32(20);
							int32& as_ast_e110_target = as_ast_d13;
							as_ast_e110_target = as_ast_e110_value;
							return as_ast_e110_target;
						}());
					}
					{
						([&]() -> int32
						{
							const int32 as_ast_e114_value = as_ast_d13;
							int32& as_ast_e114_target = as_ast_d9;
							as_ast_e114_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e114_target, as_ast_e114_value);
							return as_ast_e114_target;
						}());
					}
					break;
				}
			case int32(3):
				{
					int32 as_ast_d14;
					{
						([&]() -> int32
						{
							const int32 as_ast_e119_value = int32(0);
							int32& as_ast_e119_target = as_ast_d14;
							as_ast_e119_target = as_ast_e119_value;
							return as_ast_e119_target;
						}());
					}
					{
						while (([&]() -> bool
						{
							const int32 as_ast_e122_left = as_ast_d14;
							const int32 as_ast_e122_right = int32(2);
							return (as_ast_e122_left < as_ast_e122_right);
						}()))
						{
							{
								([&]() -> int32
								{
									const int32 as_ast_e127_value = int32(7);
									int32& as_ast_e127_target = as_ast_d9;
									as_ast_e127_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e127_target, as_ast_e127_value);
									return as_ast_e127_target;
								}());
							}
							{
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d14);
							}
							{
								if (([&]() -> bool
								{
									const int32 as_ast_e134_left = as_ast_d14;
									const int32 as_ast_e134_right = int32(1);
									return (as_ast_e134_left == as_ast_e134_right);
								}()))
								{
									continue;
								}
							}
							break;
						}
					}
					break;
				}
			default:
				{
					([&]() -> int32
					{
						const int32 as_ast_e144_value = int32(9);
						int32& as_ast_e144_target = as_ast_d9;
						as_ast_e144_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e144_target, as_ast_e144_value);
						return as_ast_e144_target;
					}());
				}
				break;
		}
	}
	return as_ast_d9;
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

static int32 AS_Test_Semantic_SemanticExhaustiveEnum(
	FScriptExecution& Execution,
	const int32 as_ast_d16)
{
	int8 as_ast_d17;
	{
		([&]() -> int8
		{
			const int8 as_ast_e151_value = AngelscriptTypedASTJIT::WrapNarrow<int8>(as_ast_d16);
			int8& as_ast_e151_target = as_ast_d17;
			as_ast_e151_target = as_ast_e151_value;
			return as_ast_e151_target;
		}());
	}
	{
		const auto as_ast_stmt_84_condition = as_ast_d17;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return {};
		}
		switch (as_ast_stmt_84_condition)
		{
			case int8(0):
				return int32(10);
			case int8(1):
				return int32(20);
			case int8(2):
				return int32(30);
			default:
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

	struct FTypedASTScalarProbeRegistration
	{
		FTypedASTScalarProbeRegistration()
		{
			AngelscriptTestJITProbes::RegisterTypedASTScalarBranchProbe(
				&AS_Test_Semantic_SemanticScalarBranch);
			AngelscriptTestJITProbes::RegisterTypedASTMutationPowerProbes(
				&AS_Test_Semantic_SemanticMutationParity,
				&AS_Test_Semantic_SemanticPowerFloat,
				&AS_Test_Semantic_SemanticPowerDouble,
				&AS_Test_Semantic_SemanticPowerDoubleInt);
			AngelscriptTestJITProbes::RegisterTypedASTControlFlowProbes(
				&AS_Test_Semantic_SemanticStructuredControlFlow,
				&AS_Test_Semantic_SemanticExhaustiveEnum);
		}
	};

	FTypedASTScalarProbeRegistration GTypedASTScalarProbeRegistration;
}
