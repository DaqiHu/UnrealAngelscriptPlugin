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
	const int32 as_ast_d11,
	const int32 as_ast_d12)
{
	int32 as_ast_d13;
	{
		([&]() -> int32
		{
			const int32 as_ast_e20_value = as_ast_d11;
			int32& as_ast_e20_target = as_ast_d13;
			as_ast_e20_target = as_ast_e20_value;
			return as_ast_e20_target;
		}());
	}
	{
		([&]() -> int32
		{
			const int32 as_ast_e23_value = as_ast_d12;
			int32& as_ast_e23_target = as_ast_d13;
			as_ast_e23_target = as_ast_e23_value;
			return as_ast_e23_target;
		}());
	}
	{
		([&]() -> int32
		{
			const int32 as_ast_e26_value = int32(3);
			int32& as_ast_e26_target = as_ast_d13;
			as_ast_e26_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e26_target, as_ast_e26_value);
			return as_ast_e26_target;
		}());
	}
	int32 as_ast_d14;
	{
		([&]() -> int32
		{
			const int32 as_ast_e31_value = AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d13);
			int32& as_ast_e31_target = as_ast_d14;
			as_ast_e31_target = as_ast_e31_value;
			return as_ast_e31_target;
		}());
	}
	int32 as_ast_d15;
	{
		([&]() -> int32
		{
			const int32 as_ast_e36_value = AngelscriptTypedASTJIT::WrapPostIncrement<int32>(as_ast_d13);
			int32& as_ast_e36_target = as_ast_d15;
			as_ast_e36_target = as_ast_e36_value;
			return as_ast_e36_target;
		}());
	}
	return ([&]() -> int32
	{
		const int32 as_ast_e45_left = ([&]() -> int32
	{
		const int32 as_ast_e44_left = ([&]() -> int32
	{
		const int32 as_ast_e42_left = as_ast_d13;
		const int32 as_ast_e42_right = int32(1000);
		return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e42_left, as_ast_e42_right);
	}());
		const int32 as_ast_e44_right = ([&]() -> int32
	{
		const int32 as_ast_e43_left = as_ast_d14;
		const int32 as_ast_e43_right = int32(100);
		return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_ast_e43_left, as_ast_e43_right);
	}());
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e44_left, as_ast_e44_right);
	}());
		const int32 as_ast_e45_right = as_ast_d15;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e45_left, as_ast_e45_right);
	}());
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return int32{};
}

static float AS_Test_Semantic_SemanticPowerFloat(
	FScriptExecution& Execution,
	const float as_ast_d17,
	const float as_ast_d18)
{
	return ([&]() -> float
	{
		const float as_ast_e48_left = as_ast_d17;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return float{};
		}
		const float as_ast_e48_right = as_ast_d18;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return float{};
		}
		return AngelscriptTypedASTJIT::CheckedPower<float>(Execution, as_ast_e48_left, as_ast_e48_right);
	}());
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return float{};
}

static double AS_Test_Semantic_SemanticPowerDouble(
	FScriptExecution& Execution,
	const double as_ast_d20,
	const double as_ast_d21)
{
	return ([&]() -> double
	{
		const double as_ast_e51_left = as_ast_d20;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		const double as_ast_e51_right = as_ast_d21;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		return AngelscriptTypedASTJIT::CheckedPower<double>(Execution, as_ast_e51_left, as_ast_e51_right);
	}());
	// Canonical Sema proves source-level return completeness; this fallback keeps C++ structurally total when it cannot prove enum switch exhaustiveness.
	return double{};
}

static double AS_Test_Semantic_SemanticPowerDoubleInt(
	FScriptExecution& Execution,
	const double as_ast_d23,
	const int32 as_ast_d24)
{
	return ([&]() -> double
	{
		const double as_ast_e55_left = as_ast_d23;
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		const double as_ast_e55_right = static_cast<double>(as_ast_d24);
		if (UNLIKELY(Execution.bExceptionThrown))
		{
			return double{};
		}
		return AngelscriptTypedASTJIT::CheckedPower<double>(Execution, as_ast_e55_left, as_ast_e55_right);
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
					const int32 as_ast_e15_left = as_ast_d10;
					const int32 as_ast_e15_right = int32(1);
					return (as_ast_e15_left == as_ast_e15_right);
				}()))
				{
					continue;
				}
			}
			{
				switch (as_ast_d10)
				{
					case static_cast<int32>(int32(2)):
						{
							([&]() -> int32
							{
								const int32 as_ast_e20_value = int32(20);
								int32& as_ast_e20_target = as_ast_d9;
								as_ast_e20_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e20_target, as_ast_e20_value);
								return as_ast_e20_target;
							}());
						}
						break;
					case static_cast<int32>(int32(3)):
						{
							([&]() -> int32
							{
								const int32 as_ast_e24_value = int32(30);
								int32& as_ast_e24_target = as_ast_d9;
								as_ast_e24_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e24_target, as_ast_e24_value);
								return as_ast_e24_target;
							}());
						}
						continue;
					default:
						{
							([&]() -> int32
							{
								const int32 as_ast_e27_value = as_ast_d10;
								int32& as_ast_e27_target = as_ast_d9;
								as_ast_e27_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e27_target, as_ast_e27_value);
								return as_ast_e27_target;
							}());
						}
						break;
				}
			}
			{
				if (([&]() -> bool
				{
					const int32 as_ast_e32_left = as_ast_d9;
					const int32 as_ast_e32_right = int32(45);
					return (as_ast_e32_left > as_ast_e32_right);
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
			const int32 as_ast_e35_value = int32(0);
			int32& as_ast_e35_target = as_ast_d11;
			as_ast_e35_target = as_ast_e35_value;
			return as_ast_e35_target;
		}());
	}
	{
		while (([&]() -> bool
		{
			const int32 as_ast_e38_left = as_ast_d11;
			const int32 as_ast_e38_right = as_ast_d7;
			return (as_ast_e38_left < as_ast_e38_right);
		}()))
		{
			{
				AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d11);
			}
			{
				if (([&]() -> bool
				{
					const int32 as_ast_e44_left = as_ast_d11;
					const int32 as_ast_e44_right = int32(2);
					return (as_ast_e44_left == as_ast_e44_right);
				}()))
				{
					continue;
				}
			}
			{
				([&]() -> int32
				{
					const int32 as_ast_e47_value = int32(3);
					int32& as_ast_e47_target = as_ast_d9;
					as_ast_e47_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e47_target, as_ast_e47_value);
					return as_ast_e47_target;
				}());
			}
			{
				if (([&]() -> bool
				{
					const int32 as_ast_e50_left = as_ast_d11;
					const int32 as_ast_e50_right = int32(4);
					return (as_ast_e50_left >= as_ast_e50_right);
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
			const int32 as_ast_e53_value = int32(0);
			int32& as_ast_e53_target = as_ast_d12;
			as_ast_e53_target = as_ast_e53_value;
			return as_ast_e53_target;
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
					const int32 as_ast_e59_left = as_ast_d12;
					const int32 as_ast_e59_right = int32(1);
					return (as_ast_e59_left == as_ast_e59_right);
				}()))
				{
					continue;
				}
			}
			{
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
		switch (as_ast_d8)
		{
			case static_cast<int32>(int32(0)):
				{
					([&]() -> int32
					{
						const int32 as_ast_e70_value = int32(100);
						int32& as_ast_e70_target = as_ast_d9;
						as_ast_e70_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e70_target, as_ast_e70_value);
						return as_ast_e70_target;
					}());
				}
				break;
			case static_cast<int32>(int32(1)):
				{
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
						([&]() -> int32
						{
							const int32 as_ast_e78_value = int32(20);
							int32& as_ast_e78_target = as_ast_d13;
							as_ast_e78_target = as_ast_e78_value;
							return as_ast_e78_target;
						}());
					}
					{
						([&]() -> int32
						{
							const int32 as_ast_e81_value = as_ast_d13;
							int32& as_ast_e81_target = as_ast_d9;
							as_ast_e81_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e81_target, as_ast_e81_value);
							return as_ast_e81_target;
						}());
					}
					break;
				}
			case static_cast<int32>(int32(3)):
				{
					int32 as_ast_d14;
					{
						([&]() -> int32
						{
							const int32 as_ast_e85_value = int32(0);
							int32& as_ast_e85_target = as_ast_d14;
							as_ast_e85_target = as_ast_e85_value;
							return as_ast_e85_target;
						}());
					}
					{
						while (([&]() -> bool
						{
							const int32 as_ast_e88_left = as_ast_d14;
							const int32 as_ast_e88_right = int32(2);
							return (as_ast_e88_left < as_ast_e88_right);
						}()))
						{
							{
								([&]() -> int32
								{
									const int32 as_ast_e91_value = int32(7);
									int32& as_ast_e91_target = as_ast_d9;
									as_ast_e91_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e91_target, as_ast_e91_value);
									return as_ast_e91_target;
								}());
							}
							{
								AngelscriptTypedASTJIT::WrapPreIncrement<int32>(as_ast_d14);
							}
							{
								if (([&]() -> bool
								{
									const int32 as_ast_e97_left = as_ast_d14;
									const int32 as_ast_e97_right = int32(1);
									return (as_ast_e97_left == as_ast_e97_right);
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
						const int32 as_ast_e100_value = int32(9);
						int32& as_ast_e100_target = as_ast_d9;
						as_ast_e100_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_ast_e100_target, as_ast_e100_value);
						return as_ast_e100_target;
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
			const int8 as_ast_e109_value = AngelscriptTypedASTJIT::WrapNarrow<int8>(as_ast_d16);
			int8& as_ast_e109_target = as_ast_d17;
			as_ast_e109_target = as_ast_e109_value;
			return as_ast_e109_target;
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
			case static_cast<int8>(int8(0)):
				return int32(10);
			case static_cast<int8>(int8(1)):
				return int32(20);
			case static_cast<int8>(int8(2)):
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
