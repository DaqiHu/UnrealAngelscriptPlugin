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
			const int32 as_ast_e8_value = ([&]() -> int32
		{
			const int32 as_ast_e6_left = as_ast_d7;
			const int32 as_ast_e6_right = as_ast_d8;
			return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e6_left, as_ast_e6_right);
		}());
			int32& as_ast_e8_target = as_ast_d9;
			as_ast_e8_target = as_ast_e8_value;
			return as_ast_e8_target;
		}());
	}
	{
		if (([&]() -> bool
		{
			const int32 as_ast_e11_left = as_ast_d9;
			const int32 as_ast_e11_right = int32(10);
			return (as_ast_e11_left > as_ast_e11_right);
		}()))
		{
			return ([&]() -> int32
			{
				const int32 as_ast_e14_left = as_ast_d9;
				const int32 as_ast_e14_right = int32(2);
				return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e14_left, as_ast_e14_right);
			}());
		}
	}
	return ([&]() -> int32
	{
		const int32 as_ast_e17_left = as_ast_d9;
		const int32 as_ast_e17_right = int32(1);
		return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_ast_e17_left, as_ast_e17_right);
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
			const int32 as_ast_e20_value = as_ast_d14;
			int32& as_ast_e20_target = as_ast_d16;
			as_ast_e20_target = as_ast_e20_value;
			return as_ast_e20_target;
		}());
	}
	{
		([&]() -> int32
		{
			const int32 as_ast_e23_value = as_ast_d15;
			int32& as_ast_e23_target = as_ast_d16;
			as_ast_e23_target = as_ast_e23_value;
			return as_ast_e23_target;
		}());
	}
	{
		([&]() -> int32
		{
			const int32 as_ast_e26_value = int32(3);
			int32& as_ast_e26_target = as_ast_d16;
			as_ast_e26_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e26_target, as_ast_e26_value);
			return as_ast_e26_target;
		}());
	}
	int32 as_ast_d17;
	{
		([&]() -> int32
		{
			const int32 as_ast_e30_value = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d16);
			int32& as_ast_e30_target = as_ast_d17;
			as_ast_e30_target = as_ast_e30_value;
			return as_ast_e30_target;
		}());
	}
	int32 as_ast_d18;
	{
		([&]() -> int32
		{
			const int32 as_ast_e34_value = AngelscriptTypedASTJIT::WrapPostIncrement<int32>(as_ast_d16);
			int32& as_ast_e34_target = as_ast_d18;
			as_ast_e34_target = as_ast_e34_value;
			return as_ast_e34_target;
		}());
	}
	return ([&]() -> int32
	{
		const int32 as_ast_e43_left = ([&]() -> int32
	{
		const int32 as_ast_e42_left = ([&]() -> int32
	{
		const int32 as_ast_e40_left = as_ast_d16;
		const int32 as_ast_e40_right = int32(1000);
		return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e40_left, as_ast_e40_right);
	}());
		const int32 as_ast_e42_right = ([&]() -> int32
	{
		const int32 as_ast_e41_left = as_ast_d17;
		const int32 as_ast_e41_right = int32(100);
		return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e41_left, as_ast_e41_right);
	}());
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e42_left, as_ast_e42_right);
	}());
		const int32 as_ast_e43_right = as_ast_d18;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e43_left, as_ast_e43_right);
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
		const float as_ast_e46_left = as_ast_d20;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return float{};
		}
		const float as_ast_e46_right = as_ast_d21;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return float{};
		}
		return AngelscriptTypedASTJIT::CheckedPower<float>(Execution, as_ast_e46_left, as_ast_e46_right);
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
		const double as_ast_e49_left = as_ast_d23;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		const double as_ast_e49_right = as_ast_d24;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		return AngelscriptTypedASTJIT::CheckedPower<double>(Execution, as_ast_e49_left, as_ast_e49_right);
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
		const double as_ast_e53_left = as_ast_d26;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		const double as_ast_e53_right = static_cast<double>(as_ast_d27);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		return AngelscriptTypedASTJIT::CheckedPower<double>(Execution, as_ast_e53_left, as_ast_e53_right);
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
								const int32 as_ast_e19_value = int32(20);
								int32& as_ast_e19_target = as_ast_d9;
								as_ast_e19_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e19_target, as_ast_e19_value);
								return as_ast_e19_target;
							}());
						}
						break;
					case int32(3):
						{
							([&]() -> int32
							{
								const int32 as_ast_e23_value = int32(30);
								int32& as_ast_e23_target = as_ast_d9;
								as_ast_e23_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e23_target, as_ast_e23_value);
								return as_ast_e23_target;
							}());
						}
						continue;
					default:
						{
							([&]() -> int32
							{
								const int32 as_ast_e26_value = as_ast_d10;
								int32& as_ast_e26_target = as_ast_d9;
								as_ast_e26_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e26_target, as_ast_e26_value);
								return as_ast_e26_target;
							}());
						}
						break;
				}
			}
			{
				if (([&]() -> bool
				{
					const int32 as_ast_e29_left = as_ast_d9;
					const int32 as_ast_e29_right = int32(45);
					return (as_ast_e29_left > as_ast_e29_right);
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
			const int32 as_ast_e32_value = int32(0);
			int32& as_ast_e32_target = as_ast_d11;
			as_ast_e32_target = as_ast_e32_value;
			return as_ast_e32_target;
		}());
	}
	{
		while (([&]() -> bool
		{
			const int32 as_ast_e35_left = as_ast_d11;
			const int32 as_ast_e35_right = as_ast_d7;
			return (as_ast_e35_left < as_ast_e35_right);
		}()))
		{
			{
				AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d11);
			}
			{
				if (([&]() -> bool
				{
					const int32 as_ast_e40_left = as_ast_d11;
					const int32 as_ast_e40_right = int32(2);
					return (as_ast_e40_left == as_ast_e40_right);
				}()))
				{
					continue;
				}
			}
			{
				([&]() -> int32
				{
					const int32 as_ast_e43_value = int32(3);
					int32& as_ast_e43_target = as_ast_d9;
					as_ast_e43_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e43_target, as_ast_e43_value);
					return as_ast_e43_target;
				}());
			}
			{
				if (([&]() -> bool
				{
					const int32 as_ast_e46_left = as_ast_d11;
					const int32 as_ast_e46_right = int32(4);
					return (as_ast_e46_left >= as_ast_e46_right);
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
			const int32 as_ast_e49_value = int32(0);
			int32& as_ast_e49_target = as_ast_d12;
			as_ast_e49_target = as_ast_e49_value;
			return as_ast_e49_target;
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
					const int32 as_ast_e54_left = as_ast_d12;
					const int32 as_ast_e54_right = int32(1);
					return (as_ast_e54_left == as_ast_e54_right);
				}()))
				{
					continue;
				}
			}
			{
				([&]() -> int32
				{
					const int32 as_ast_e57_value = int32(5);
					int32& as_ast_e57_target = as_ast_d9;
					as_ast_e57_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e57_target, as_ast_e57_value);
					return as_ast_e57_target;
				}());
			}
		}
		while (([&]() -> bool
		{
			const int32 as_ast_e60_left = as_ast_d12;
			const int32 as_ast_e60_right = int32(2);
			return (as_ast_e60_left < as_ast_e60_right);
		}()));
	}
	{
		switch (as_ast_d8)
		{
			case int32(0):
				{
					([&]() -> int32
					{
						const int32 as_ast_e65_value = int32(100);
						int32& as_ast_e65_target = as_ast_d9;
						as_ast_e65_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e65_target, as_ast_e65_value);
						return as_ast_e65_target;
					}());
				}
				break;
			case int32(1):
				{
					([&]() -> int32
					{
						const int32 as_ast_e69_value = int32(200);
						int32& as_ast_e69_target = as_ast_d9;
						as_ast_e69_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e69_target, as_ast_e69_value);
						return as_ast_e69_target;
					}());
				}
			case int32(2):
				{
					int32 as_ast_d13;
					{
						([&]() -> int32
						{
							const int32 as_ast_e73_value = int32(20);
							int32& as_ast_e73_target = as_ast_d13;
							as_ast_e73_target = as_ast_e73_value;
							return as_ast_e73_target;
						}());
					}
					{
						([&]() -> int32
						{
							const int32 as_ast_e76_value = as_ast_d13;
							int32& as_ast_e76_target = as_ast_d9;
							as_ast_e76_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e76_target, as_ast_e76_value);
							return as_ast_e76_target;
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
							const int32 as_ast_e80_value = int32(0);
							int32& as_ast_e80_target = as_ast_d14;
							as_ast_e80_target = as_ast_e80_value;
							return as_ast_e80_target;
						}());
					}
					{
						while (([&]() -> bool
						{
							const int32 as_ast_e83_left = as_ast_d14;
							const int32 as_ast_e83_right = int32(2);
							return (as_ast_e83_left < as_ast_e83_right);
						}()))
						{
							{
								([&]() -> int32
								{
									const int32 as_ast_e86_value = int32(7);
									int32& as_ast_e86_target = as_ast_d9;
									as_ast_e86_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e86_target, as_ast_e86_value);
									return as_ast_e86_target;
								}());
							}
							{
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d14);
							}
							{
								if (([&]() -> bool
								{
									const int32 as_ast_e91_left = as_ast_d14;
									const int32 as_ast_e91_right = int32(1);
									return (as_ast_e91_left == as_ast_e91_right);
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
						const int32 as_ast_e94_value = int32(9);
						int32& as_ast_e94_target = as_ast_d9;
						as_ast_e94_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e94_target, as_ast_e94_value);
						return as_ast_e94_target;
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
			const int8 as_ast_e99_value = AngelscriptTypedASTJIT::WrapNarrow<int8>(as_ast_d16);
			int8& as_ast_e99_target = as_ast_d17;
			as_ast_e99_target = as_ast_e99_value;
			return as_ast_e99_target;
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
