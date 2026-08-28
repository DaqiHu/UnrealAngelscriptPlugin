#pragma once

#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Math/NumericLimits.h"
#include "StaticJIT/StaticJITHeader.h"
#include "Templates/IsIntegral.h"
#include "Templates/IsSigned.h"
#include "Templates/MakeUnsigned.h"
#include "Templates/TypeCompatibleBytes.h"

#include <type_traits>

namespace AngelscriptTypedASTJIT
{
	template <typename ScalarType>
	FORCEINLINE ScalarType FromCanonicalBits(const uint64 Bits)
	{
		if constexpr (std::is_same_v<ScalarType, bool>)
		{
			return Bits != 0;
		}
		else if constexpr (std::is_same_v<ScalarType, float>)
		{
			return BitCast<float>(static_cast<uint32>(Bits));
		}
		else if constexpr (std::is_same_v<ScalarType, double>)
		{
			return BitCast<double>(Bits);
		}
		else
		{
			static_assert(TIsIntegral<ScalarType>::Value,
				"TypedASTJIT canonical bits require a scalar value type");
			using UnsignedType = typename TMakeUnsigned<ScalarType>::Type;
			const UnsignedType NarrowBits =
				static_cast<UnsignedType>(Bits);
			if constexpr (TIsSigned<ScalarType>::Value)
			{
				return BitCast<ScalarType>(NarrowBits);
			}
			else
			{
				return NarrowBits;
			}
		}
	}

	template <typename ScalarType>
	FORCEINLINE typename TMakeUnsigned<ScalarType>::Type ToUnsignedBits(
		const ScalarType Value)
	{
		using UnsignedType = typename TMakeUnsigned<ScalarType>::Type;
		return static_cast<UnsignedType>(Value);
	}

	template <typename ScalarType>
	FORCEINLINE ScalarType FromUnsignedBits(
		const typename TMakeUnsigned<ScalarType>::Type Bits)
	{
		if constexpr (TIsSigned<ScalarType>::Value)
		{
			return BitCast<ScalarType>(Bits);
		}
		else
		{
			return Bits;
		}
	}

	template <typename TargetType, typename SourceType>
	FORCEINLINE TargetType ConvertInteger(const SourceType Value)
	{
		static_assert(TIsIntegral<TargetType>::Value,
			"TypedASTJIT integer conversion target must be integral");
		static_assert(TIsIntegral<SourceType>::Value,
			"TypedASTJIT integer conversion source must be integral");
		static_assert(!std::is_same_v<std::remove_cv_t<TargetType>, bool>,
			"TypedASTJIT integer conversion does not normalize bool targets");
		static_assert(!std::is_same_v<std::remove_cv_t<SourceType>, bool>,
			"TypedASTJIT integer conversion does not accept bool sources");
		using UnsignedTarget = typename TMakeUnsigned<TargetType>::Type;
		const UnsignedTarget TargetBits = static_cast<UnsignedTarget>(
			static_cast<uint64>(Value));
		return FromUnsignedBits<TargetType>(TargetBits);
	}

	template <typename TargetType, typename SourceType>
	FORCEINLINE TargetType WrapNarrow(const SourceType Value)
	{
		static_assert(TIsIntegral<TargetType>::Value,
			"TypedASTJIT narrowing target must be integral");
		static_assert(TIsIntegral<SourceType>::Value,
			"TypedASTJIT narrowing source must be integral");
		static_assert(sizeof(TargetType) < sizeof(SourceType),
			"TypedASTJIT WrapNarrow is reserved for true narrowing");
		using UnsignedTarget = typename TMakeUnsigned<TargetType>::Type;
		const UnsignedTarget Bits = static_cast<UnsignedTarget>(
			ToUnsignedBits(Value));
		return FromUnsignedBits<TargetType>(Bits);
	}

	template <typename ScalarType>
	FORCEINLINE ScalarType WrapAdd(
		const ScalarType Left,
		const ScalarType Right)
	{
		using UnsignedType = typename TMakeUnsigned<ScalarType>::Type;
		const UnsignedType Result = static_cast<UnsignedType>(
			static_cast<uint64>(ToUnsignedBits(Left))
			+ static_cast<uint64>(ToUnsignedBits(Right)));
		return FromUnsignedBits<ScalarType>(Result);
	}

	template <typename ScalarType>
	FORCEINLINE ScalarType WrapSubtract(
		const ScalarType Left,
		const ScalarType Right)
	{
		using UnsignedType = typename TMakeUnsigned<ScalarType>::Type;
		const UnsignedType Result = static_cast<UnsignedType>(
			static_cast<uint64>(ToUnsignedBits(Left))
			- static_cast<uint64>(ToUnsignedBits(Right)));
		return FromUnsignedBits<ScalarType>(Result);
	}

	template <typename ScalarType>
	FORCEINLINE ScalarType WrapMultiply(
		const ScalarType Left,
		const ScalarType Right)
	{
		using UnsignedType = typename TMakeUnsigned<ScalarType>::Type;
		const UnsignedType Result = static_cast<UnsignedType>(
			static_cast<uint64>(ToUnsignedBits(Left))
			* static_cast<uint64>(ToUnsignedBits(Right)));
		return FromUnsignedBits<ScalarType>(Result);
	}

	template <typename ScalarType>
	FORCEINLINE ScalarType WrapPreIncrement(ScalarType& Value)
	{
		static_assert(TIsIntegral<ScalarType>::Value,
			"TypedASTJIT wrapping increment supports integral scalars only");
		Value = WrapAdd<ScalarType>(Value, ScalarType{1});
		return Value;
	}

	template <typename ScalarType>
	FORCEINLINE ScalarType WrapPostIncrement(ScalarType& Value)
	{
		static_assert(TIsIntegral<ScalarType>::Value,
			"TypedASTJIT wrapping increment supports integral scalars only");
		const ScalarType Previous = Value;
		Value = WrapAdd<ScalarType>(Value, ScalarType{1});
		return Previous;
	}

	template <typename ScalarType>
	FORCEINLINE ScalarType WrapPreDecrement(ScalarType& Value)
	{
		static_assert(TIsIntegral<ScalarType>::Value,
			"TypedASTJIT wrapping decrement supports integral scalars only");
		Value = WrapSubtract<ScalarType>(Value, ScalarType{1});
		return Value;
	}

	template <typename ScalarType>
	FORCEINLINE ScalarType WrapPostDecrement(ScalarType& Value)
	{
		static_assert(TIsIntegral<ScalarType>::Value,
			"TypedASTJIT wrapping decrement supports integral scalars only");
		const ScalarType Previous = Value;
		Value = WrapSubtract<ScalarType>(Value, ScalarType{1});
		return Previous;
	}

	template <typename ScalarType>
	FORCEINLINE bool IsDivisionOverflow(
		const ScalarType Left,
		const ScalarType Right)
	{
		static_assert(TIsIntegral<ScalarType>::Value,
			"TypedASTJIT checked division supports integral scalars only");
		if constexpr (TIsSigned<ScalarType>::Value)
		{
			return Left == TNumericLimits<ScalarType>::Min()
				&& Right == ScalarType{-1};
		}
		return false;
	}

	template <typename ScalarType>
	FORCEINLINE ScalarType CheckedDivide(
		FScriptExecution& Execution,
		const ScalarType Left,
		const ScalarType Right)
	{
		static_assert(TIsIntegral<ScalarType>::Value,
			"TypedASTJIT checked division supports integral scalars only");
		if (Execution.bExceptionThrown)
		{
			return ScalarType{0};
		}
		if (Right == ScalarType{0})
		{
			FStaticJITFunction::SetDivByZeroException(Execution);
			return ScalarType{0};
		}
		if (IsDivisionOverflow(Left, Right))
		{
			FStaticJITFunction::SetOverflowException(Execution);
			return ScalarType{0};
		}
		return Left / Right;
	}

	template <typename ScalarType>
	FORCEINLINE ScalarType CheckedRemainder(
		FScriptExecution& Execution,
		const ScalarType Left,
		const ScalarType Right)
	{
		static_assert(TIsIntegral<ScalarType>::Value,
			"TypedASTJIT checked remainder supports integral scalars only");
		if (Execution.bExceptionThrown)
		{
			return ScalarType{0};
		}
		if (Right == ScalarType{0})
		{
			FStaticJITFunction::SetDivByZeroException(Execution);
			return ScalarType{0};
		}
		if (IsDivisionOverflow(Left, Right))
		{
			FStaticJITFunction::SetOverflowException(Execution);
			return ScalarType{0};
		}
		return Left % Right;
	}

	template <typename ResultType, typename BaseType, typename ExponentType>
	FORCEINLINE ResultType CheckedPower(
		FScriptExecution& Execution,
		const BaseType Base,
		const ExponentType Exponent)
	{
		static_assert(std::is_floating_point_v<ResultType>,
			"TypedASTJIT checked power requires a floating-point result");
		if (Execution.bExceptionThrown)
		{
			return ResultType{0};
		}
		const ResultType Result = FMath::Pow(
			static_cast<ResultType>(Base),
			static_cast<ResultType>(Exponent));
		if (Result == static_cast<ResultType>(HUGE_VAL))
		{
			FStaticJITFunction::SetPowerOverflowException(Execution);
		}
		return Result;
	}

	template <typename ScalarType>
	FORCEINLINE ScalarType WrapNegate(const ScalarType Value)
	{
		using UnsignedType = typename TMakeUnsigned<ScalarType>::Type;
		const UnsignedType Result = static_cast<UnsignedType>(
			uint64{0} - static_cast<uint64>(ToUnsignedBits(Value)));
		return FromUnsignedBits<ScalarType>(Result);
	}

	template <typename ScalarType>
	FORCEINLINE ScalarType BitwiseNot(const ScalarType Value)
	{
		using UnsignedType = typename TMakeUnsigned<ScalarType>::Type;
		return FromUnsignedBits<ScalarType>(static_cast<UnsignedType>(
			~ToUnsignedBits(Value)));
	}

	template <typename ScalarType, typename CountType>
	FORCEINLINE uint32 MaskShiftCount(const CountType RawCount)
	{
		constexpr uint32 Width = sizeof(ScalarType) * 8;
		return static_cast<uint32>(static_cast<uint64>(RawCount))
			& (Width - 1);
	}

	template <typename ScalarType, typename CountType>
	FORCEINLINE ScalarType ShiftLeft(
		const ScalarType Value,
		const CountType RawCount)
	{
		using UnsignedType = typename TMakeUnsigned<ScalarType>::Type;
		const uint32 Count = MaskShiftCount<ScalarType>(RawCount);
		const UnsignedType Result = static_cast<UnsignedType>(
			static_cast<uint64>(ToUnsignedBits(Value)) << Count);
		return FromUnsignedBits<ScalarType>(Result);
	}

	template <typename ScalarType, typename CountType>
	FORCEINLINE ScalarType LogicalShiftRight(
		const ScalarType Value,
		const CountType RawCount)
	{
		using UnsignedType = typename TMakeUnsigned<ScalarType>::Type;
		const uint32 Count = MaskShiftCount<ScalarType>(RawCount);
		const UnsignedType Result = static_cast<UnsignedType>(
			static_cast<uint64>(ToUnsignedBits(Value)) >> Count);
		return FromUnsignedBits<ScalarType>(Result);
	}

	template <typename ScalarType, typename CountType>
	FORCEINLINE ScalarType ArithmeticShiftRight(
		const ScalarType Value,
		const CountType RawCount)
	{
		using UnsignedType = typename TMakeUnsigned<ScalarType>::Type;
		constexpr uint32 Width = sizeof(ScalarType) * 8;
		const uint32 Count = MaskShiftCount<ScalarType>(RawCount);
		const uint64 WidthMask = MAX_uint64 >> (64 - Width);
		const uint64 Bits = static_cast<uint64>(ToUnsignedBits(Value));
		if (Count == 0)
		{
			return Value;
		}
		const uint64 Logical = Bits >> Count;
		const bool bNegative = (Bits & (uint64{1} << (Width - 1))) != 0;
		const uint64 Fill = bNegative
			? (~uint64{0} << (Width - Count)) & WidthMask
			: uint64{0};
		return FromUnsignedBits<ScalarType>(
			static_cast<UnsignedType>((Logical | Fill) & WidthMask));
	}
}
