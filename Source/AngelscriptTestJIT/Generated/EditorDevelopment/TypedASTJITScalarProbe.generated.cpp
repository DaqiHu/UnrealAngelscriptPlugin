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
#include "StaticJIT/StaticJITHeader.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITCallBridge.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"

namespace
{
static int32 AS_Test_Semantic_SemanticScalarBranch(
	const int32 as_sem_s0,
	const int32 as_sem_s1)
{
	const int32 as_sem_s2 = ([&]() -> int32
	{
		const int32 as_sem_e2_left = as_sem_s0;
		const int32 as_sem_e2_right = as_sem_s1;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e2_left, as_sem_e2_right);
	}());
	if (([&]() -> bool
	{
		const int32 as_sem_e5_left = as_sem_s2;
		const int32 as_sem_e5_right = int32(10);
		return (as_sem_e5_left > as_sem_e5_right);
	}()))
	{
		return ([&]() -> int32
		{
			const int32 as_sem_e8_left = as_sem_s2;
			const int32 as_sem_e8_right = int32(2);
			return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e8_left, as_sem_e8_right);
		}());
	}
	return ([&]() -> int32
	{
		const int32 as_sem_e11_left = as_sem_s2;
		const int32 as_sem_e11_right = int32(1);
		return AngelscriptTypedASTJIT::WrapSubtract<int32>(as_sem_e11_left, as_sem_e11_right);
	}());
}

static int32 AS_Test_Semantic_SemanticMutationParity(
	const int32 as_sem_s0,
	const int32 as_sem_s1)
{
	int32 as_sem_s2 = as_sem_s0;
	([&]() -> int32
	{
		const int32 as_sem_e3_value = as_sem_s1;
		int32& as_sem_e3_target = as_sem_s2;
		as_sem_e3_target = as_sem_e3_value;
		return as_sem_e3_target;
	}());
	([&]() -> int32
	{
		const int32 as_sem_e6_value = int32(3);
		int32& as_sem_e6_target = as_sem_s2;
		as_sem_e6_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e6_target, as_sem_e6_value);
		return as_sem_e6_target;
	}());
	const int32 as_sem_s3 = ([&]() -> int32
	{
		int32& as_sem_e8_target = as_sem_s2;
		as_sem_e8_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e8_target, int32(1));
		return as_sem_e8_target;
	}());
	const int32 as_sem_s4 = ([&]() -> int32
	{
		int32& as_sem_e10_target = as_sem_s2;
		const int32 as_sem_e10_old = as_sem_e10_target;
		as_sem_e10_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e10_old, int32(1));
		return as_sem_e10_old;
	}());
	return ([&]() -> int32
	{
		const int32 as_sem_e19_left = ([&]() -> int32
	{
		const int32 as_sem_e17_left = ([&]() -> int32
	{
		const int32 as_sem_e13_left = as_sem_s2;
		const int32 as_sem_e13_right = int32(1000);
		return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e13_left, as_sem_e13_right);
	}());
		const int32 as_sem_e17_right = ([&]() -> int32
	{
		const int32 as_sem_e16_left = as_sem_s3;
		const int32 as_sem_e16_right = int32(100);
		return AngelscriptTypedASTJIT::WrapMultiply<int32>(as_sem_e16_left, as_sem_e16_right);
	}());
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e17_left, as_sem_e17_right);
	}());
		const int32 as_sem_e19_right = as_sem_s4;
		return AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e19_left, as_sem_e19_right);
	}());
}

static float AS_Test_Semantic_SemanticPowerFloat(
	FScriptExecution& Execution,
	const float as_sem_s0,
	const float as_sem_s1)
{
	return ([&]() -> float
	{
		const float as_sem_e2_left = as_sem_s0;
		const float as_sem_e2_right = as_sem_s1;
		return AngelscriptTypedASTJIT::CheckedPower<float>(Execution, as_sem_e2_left, as_sem_e2_right);
	}());
}

static double AS_Test_Semantic_SemanticPowerDouble(
	FScriptExecution& Execution,
	const double as_sem_s0,
	const double as_sem_s1)
{
	return ([&]() -> double
	{
		const double as_sem_e2_left = as_sem_s0;
		const double as_sem_e2_right = as_sem_s1;
		return AngelscriptTypedASTJIT::CheckedPower<double>(Execution, as_sem_e2_left, as_sem_e2_right);
	}());
}

static double AS_Test_Semantic_SemanticPowerDoubleInt(
	FScriptExecution& Execution,
	const double as_sem_s0,
	const int32 as_sem_s1)
{
	return ([&]() -> double
	{
		const double as_sem_e2_left = as_sem_s0;
		const int32 as_sem_e2_right = as_sem_s1;
		return AngelscriptTypedASTJIT::CheckedPower<double>(Execution, as_sem_e2_left, as_sem_e2_right);
	}());
}

static int32 AS_Test_Semantic_SemanticStructuredControlFlow(
	const int32 as_sem_s0,
	const int32 as_sem_s1)
{
	int32 as_sem_s2 = int32(0);
	{
		int32 as_sem_s3 = int32(0);
		for (; ([&]() -> bool
				{
					const int32 as_sem_e4_left = as_sem_s3;
					const int32 as_sem_e4_right = as_sem_s0;
					return (as_sem_e4_left < as_sem_e4_right);
				}()); ([&]() -> int32
			{
				int32& as_sem_e6_target = as_sem_s3;
				as_sem_e6_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e6_target, int32(1));
				return as_sem_e6_target;
			}()))
		{
			if (([&]() -> bool
			{
				const int32 as_sem_e9_left = as_sem_s3;
				const int32 as_sem_e9_right = int32(1);
				return (as_sem_e9_left == as_sem_e9_right);
			}()))
			{
				continue;
			}
			const int32 as_sem_switch_selector_19 = static_cast<int32>(as_sem_s3);
			switch (as_sem_switch_selector_19)
			{
				case int32(2):
					{
						([&]() -> int32
						{
							const int32 as_sem_e15_value = int32(20);
							int32& as_sem_e15_target = as_sem_s2;
							as_sem_e15_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e15_target, as_sem_e15_value);
							return as_sem_e15_target;
						}());
						break;
					}
					break;
				case int32(3):
					{
						([&]() -> int32
						{
							const int32 as_sem_e18_value = int32(30);
							int32& as_sem_e18_target = as_sem_s2;
							as_sem_e18_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e18_target, as_sem_e18_value);
							return as_sem_e18_target;
						}());
						continue;
					}
					break;
				default:
					{
						([&]() -> int32
						{
							const int32 as_sem_e21_value = as_sem_s3;
							int32& as_sem_e21_target = as_sem_s2;
							as_sem_e21_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e21_target, as_sem_e21_value);
							return as_sem_e21_target;
						}());
						break;
					}
					break;
			}
			if (([&]() -> bool
			{
				const int32 as_sem_e24_left = as_sem_s2;
				const int32 as_sem_e24_right = int32(45);
				return (as_sem_e24_left > as_sem_e24_right);
			}()))
			{
				break;
			}
		}
	}
	int32 as_sem_s4 = int32(0);
	while (([&]() -> bool
			{
				const int32 as_sem_e28_left = as_sem_s4;
				const int32 as_sem_e28_right = as_sem_s0;
				return (as_sem_e28_left < as_sem_e28_right);
			}()))
	{
		([&]() -> int32
		{
			int32& as_sem_e30_target = as_sem_s4;
			as_sem_e30_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e30_target, int32(1));
			return as_sem_e30_target;
		}());
		if (([&]() -> bool
		{
			const int32 as_sem_e33_left = as_sem_s4;
			const int32 as_sem_e33_right = int32(2);
			return (as_sem_e33_left == as_sem_e33_right);
		}()))
		{
			continue;
		}
		([&]() -> int32
		{
			const int32 as_sem_e36_value = int32(3);
			int32& as_sem_e36_target = as_sem_s2;
			as_sem_e36_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e36_target, as_sem_e36_value);
			return as_sem_e36_target;
		}());
		if (([&]() -> bool
		{
			const int32 as_sem_e39_left = as_sem_s4;
			const int32 as_sem_e39_right = int32(4);
			return (as_sem_e39_left >= as_sem_e39_right);
		}()))
		{
			break;
		}
	}
	int32 as_sem_s5 = int32(0);
	do
	{
		([&]() -> int32
		{
			int32& as_sem_e42_target = as_sem_s5;
			as_sem_e42_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e42_target, int32(1));
			return as_sem_e42_target;
		}());
		if (([&]() -> bool
		{
			const int32 as_sem_e45_left = as_sem_s5;
			const int32 as_sem_e45_right = int32(1);
			return (as_sem_e45_left == as_sem_e45_right);
		}()))
		{
			continue;
		}
		([&]() -> int32
		{
			const int32 as_sem_e48_value = int32(5);
			int32& as_sem_e48_target = as_sem_s2;
			as_sem_e48_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e48_target, as_sem_e48_value);
			return as_sem_e48_target;
		}());
	}
	while (([&]() -> bool
			{
				const int32 as_sem_e51_left = as_sem_s5;
				const int32 as_sem_e51_right = int32(2);
				return (as_sem_e51_left < as_sem_e51_right);
			}()));
	const int32 as_sem_switch_selector_72 = static_cast<int32>(as_sem_s1);
	switch (as_sem_switch_selector_72)
	{
		case int32(0):
			{
				([&]() -> int32
				{
					const int32 as_sem_e59_value = int32(100);
					int32& as_sem_e59_target = as_sem_s2;
					as_sem_e59_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e59_target, as_sem_e59_value);
					return as_sem_e59_target;
				}());
				break;
			}
			break;
		case int32(1):
			{
				([&]() -> int32
				{
					const int32 as_sem_e62_value = int32(200);
					int32& as_sem_e62_target = as_sem_s2;
					as_sem_e62_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e62_target, as_sem_e62_value);
					return as_sem_e62_target;
				}());
			}
		case int32(2):
			{
				const int32 as_sem_s6 = int32(20);
				([&]() -> int32
				{
					const int32 as_sem_e66_value = as_sem_s6;
					int32& as_sem_e66_target = as_sem_s2;
					as_sem_e66_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e66_target, as_sem_e66_value);
					return as_sem_e66_target;
				}());
				break;
			}
			break;
		case int32(3):
			{
				int32 as_sem_s7 = int32(0);
				while (([&]() -> bool
						{
							const int32 as_sem_e70_left = as_sem_s7;
							const int32 as_sem_e70_right = int32(2);
							return (as_sem_e70_left < as_sem_e70_right);
						}()))
				{
					([&]() -> int32
					{
						const int32 as_sem_e73_value = int32(7);
						int32& as_sem_e73_target = as_sem_s2;
						as_sem_e73_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e73_target, as_sem_e73_value);
						return as_sem_e73_target;
					}());
					([&]() -> int32
					{
						int32& as_sem_e75_target = as_sem_s7;
						as_sem_e75_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e75_target, int32(1));
						return as_sem_e75_target;
					}());
					if (([&]() -> bool
					{
						const int32 as_sem_e78_left = as_sem_s7;
						const int32 as_sem_e78_right = int32(1);
						return (as_sem_e78_left == as_sem_e78_right);
					}()))
					{
						continue;
					}
					break;
				}
				break;
			}
			break;
		default:
			{
				([&]() -> int32
				{
					const int32 as_sem_e81_value = int32(9);
					int32& as_sem_e81_target = as_sem_s2;
					as_sem_e81_target = AngelscriptTypedASTJIT::WrapAdd<int32>(as_sem_e81_target, as_sem_e81_value);
					return as_sem_e81_target;
				}());
				break;
			}
			break;
	}
	return as_sem_s2;
}

static int32 AS_Test_Semantic_SemanticExhaustiveEnum(
	FScriptExecution& Execution,
	const int32 as_sem_s0)
{
	const int8 as_sem_s1 = AngelscriptTypedASTJIT::WrapNarrow<int8>(as_sem_s0);
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	const int32 as_sem_switch_selector_10 = static_cast<int32>(as_sem_s1);
	if (UNLIKELY(Execution.bExceptionThrown))
	{
		return int32{};
	}
	switch (as_sem_switch_selector_10)
	{
		case int32(0):
			{
				return int32(10);
			}
			break;
		case int32(1):
			{
				return int32(20);
			}
			break;
		case int32(2):
			{
				return int32(30);
			}
			break;
		default:
			FStaticJITFunction::SetSwitchValueInvalidException(Execution);
			return int32{};
	}
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
