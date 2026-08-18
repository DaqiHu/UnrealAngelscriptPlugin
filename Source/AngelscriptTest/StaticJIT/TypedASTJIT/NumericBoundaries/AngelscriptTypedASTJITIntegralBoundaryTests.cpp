#include "CQTest.h"

#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"
#include "Testing/AngelscriptScriptTestRunner.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITIntegralBoundaryTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.NumericBoundaries.IntegralHelpers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	template <typename ScalarType>
	static FString TypeLabel()
	{
		return FString::Printf(
			TEXT("%s%d"),
			TIsSigned<ScalarType>::Value ? TEXT("int") : TEXT("uint"),
			static_cast<int32>(sizeof(ScalarType) * 8));
	}

	template <typename ScalarType>
	static uint64 UnsignedBits(const ScalarType Value)
	{
		return static_cast<uint64>(
			AngelscriptTypedASTJIT::ToUnsignedBits(Value));
	}

	template <typename ScalarType>
	static constexpr uint64 WidthMask()
	{
		if constexpr (sizeof(ScalarType) == sizeof(uint64))
		{
			return MAX_uint64;
		}
		else
		{
			return (uint64{1} << (sizeof(ScalarType) * 8)) - 1;
		}
	}

	template <typename ScalarType>
	static void VerifyWrappingArithmetic(FAutomationTestBase& Test)
	{
		using namespace AngelscriptTypedASTJIT;
		using UnsignedType = typename TMakeUnsigned<ScalarType>::Type;
		constexpr uint32 Width = sizeof(ScalarType) * 8;
		const uint64 WidthMask =
			FAngelscriptTypedASTJITIntegralBoundaryTests::
				WidthMask<ScalarType>();
		const ScalarType Minimum = TNumericLimits<ScalarType>::Min();
		const ScalarType Maximum = TNumericLimits<ScalarType>::Max();
		const FString Label = TypeLabel<ScalarType>();

		const uint64 ExpectedAdd = TIsSigned<ScalarType>::Value
			? uint64{1} << (Width - 1)
			: uint64{0};
		Test.TestEqual(
			*FString::Printf(TEXT("%s max+1 must wrap in its own bit domain"), *Label),
			UnsignedBits(WrapAdd<ScalarType>(Maximum, ScalarType{1})),
			ExpectedAdd);

		const uint64 ExpectedSubtract = TIsSigned<ScalarType>::Value
			? (uint64{1} << (Width - 1)) - 1
			: WidthMask;
		Test.TestEqual(
			*FString::Printf(TEXT("%s min-1 must wrap in its own bit domain"), *Label),
			UnsignedBits(WrapSubtract<ScalarType>(Minimum, ScalarType{1})),
			ExpectedSubtract);

		Test.TestEqual(
			*FString::Printf(TEXT("%s max*2 must wrap in its own bit domain"), *Label),
			UnsignedBits(WrapMultiply<ScalarType>(Maximum, ScalarType{2})),
			static_cast<uint64>(
				static_cast<UnsignedType>(WidthMask - 1)));
	}

	template <typename ScalarType>
	static void VerifyShiftCounts(FAutomationTestBase& Test)
	{
		using namespace AngelscriptTypedASTJIT;
		constexpr uint32 Width = sizeof(ScalarType) * 8;
		const uint64 WidthMask =
			FAngelscriptTypedASTJITIntegralBoundaryTests::
				WidthMask<ScalarType>();
		const uint64 ValueBits =
			(uint64{1} << (Width - 1)) | uint64{3};
		const ScalarType Value = FromUnsignedBits<ScalarType>(
			static_cast<typename TMakeUnsigned<ScalarType>::Type>(ValueBits));
		const int64 Counts[] = {
			-1,
			0,
			static_cast<int64>(Width - 1),
			static_cast<int64>(Width),
			static_cast<int64>(Width + 1),
			100000,
		};
		const FString Label = TypeLabel<ScalarType>();
		for (const int64 RawCount : Counts)
		{
			const uint32 Count =
				static_cast<uint32>(static_cast<uint64>(RawCount))
				& (Width - 1);
			const uint64 ExpectedLeft =
				(ValueBits << Count) & WidthMask;
			const uint64 ExpectedLogical = ValueBits >> Count;
			uint64 ExpectedArithmetic = ExpectedLogical;
			if (Count != 0)
			{
				ExpectedArithmetic |=
					(~uint64{0} << (Width - Count)) & WidthMask;
			}

			Test.TestEqual(
				*FString::Printf(TEXT("%s left shift count %lld must be width-masked"), *Label, RawCount),
				UnsignedBits(ShiftLeft<ScalarType>(Value, RawCount)),
				ExpectedLeft);
			Test.TestEqual(
				*FString::Printf(TEXT("%s logical right count %lld must be width-masked"), *Label, RawCount),
				UnsignedBits(LogicalShiftRight<ScalarType>(Value, RawCount)),
				ExpectedLogical);
			Test.TestEqual(
				*FString::Printf(TEXT("%s arithmetic right count %lld must use explicit sign fill"), *Label, RawCount),
				UnsignedBits(ArithmeticShiftRight<ScalarType>(Value, RawCount)),
				ExpectedArithmetic);
		}
	}

	template <typename ScalarType>
	static void VerifyZeroFailures(FAutomationTestBase& Test)
	{
		using namespace AngelscriptTypedASTJIT;
		const FString Label = TypeLabel<ScalarType>();
		{
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			ScalarType Result = ScalarType{1};
			{
				FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
				Result = CheckedDivide<ScalarType>(
					Execution, ScalarType{7}, ScalarType{0});
			}
			Test.TestTrue(
				*FString::Printf(TEXT("%s divide-by-zero must fail"), *Label),
				Execution.bExceptionThrown);
			Test.TestEqual(
				*FString::Printf(TEXT("%s divide-by-zero must return zero"), *Label),
				UnsignedBits(Result),
				uint64{0});
			Test.TestEqual(
				*FString::Printf(TEXT("%s divide-by-zero must use the shared message"), *Label),
				FString(UTF8_TO_TCHAR(
					Execution.FailureRecord != nullptr
						? Execution.FailureRecord->Message.AddressOf()
						: "")),
				FString(TEXT("Divide by zero")));
		}
		{
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			ScalarType Result = ScalarType{1};
			{
				FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
				Result = CheckedRemainder<ScalarType>(
					Execution, ScalarType{7}, ScalarType{0});
			}
			Test.TestTrue(
				*FString::Printf(TEXT("%s remainder-by-zero must fail"), *Label),
				Execution.bExceptionThrown);
			Test.TestEqual(
				*FString::Printf(TEXT("%s remainder-by-zero must return zero"), *Label),
				UnsignedBits(Result),
				uint64{0});
			Test.TestEqual(
				*FString::Printf(TEXT("%s remainder-by-zero must use the shared message"), *Label),
				FString(UTF8_TO_TCHAR(
					Execution.FailureRecord != nullptr
						? Execution.FailureRecord->Message.AddressOf()
						: "")),
				FString(TEXT("Divide by zero")));
		}
	}

	template <typename ScalarType>
	static void VerifySignedOverflowFailures(FAutomationTestBase& Test)
	{
		using namespace AngelscriptTypedASTJIT;
		static_assert(TIsSigned<ScalarType>::Value);
		const FString Label = TypeLabel<ScalarType>();
		for (const bool bRemainder : {false, true})
		{
			FScriptExecution Execution(asCThreadManager::GetLocalData());
			ScalarType Result = ScalarType{1};
			{
				FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
				Result = bRemainder
					? CheckedRemainder<ScalarType>(
						Execution,
						TNumericLimits<ScalarType>::Min(),
						ScalarType{-1})
					: CheckedDivide<ScalarType>(
						Execution,
						TNumericLimits<ScalarType>::Min(),
						ScalarType{-1});
			}
			const TCHAR* const Operation =
				bRemainder ? TEXT("remainder") : TEXT("divide");
			Test.TestTrue(
				*FString::Printf(TEXT("%s MIN/-1 %s must fail"), *Label, Operation),
				Execution.bExceptionThrown);
			Test.TestEqual(
				*FString::Printf(TEXT("%s MIN/-1 %s must return zero"), *Label, Operation),
				UnsignedBits(Result),
				uint64{0});
			Test.TestEqual(
				*FString::Printf(TEXT("%s MIN/-1 %s must use the shared overflow message"), *Label, Operation),
				FString(UTF8_TO_TCHAR(
					Execution.FailureRecord != nullptr
						? Execution.FailureRecord->Message.AddressOf()
						: "")),
				FString(TEXT("Overflow in integer division")));
		}
	}

public:
	TEST_METHOD(WrappingArithmeticCoversEveryIntegralWidthAndSignedness)
	{
		VerifyWrappingArithmetic<int8>(*TestRunner);
		VerifyWrappingArithmetic<uint8>(*TestRunner);
		VerifyWrappingArithmetic<int16>(*TestRunner);
		VerifyWrappingArithmetic<uint16>(*TestRunner);
		VerifyWrappingArithmetic<int32>(*TestRunner);
		VerifyWrappingArithmetic<uint32>(*TestRunner);
		VerifyWrappingArithmetic<int64>(*TestRunner);
		VerifyWrappingArithmetic<uint64>(*TestRunner);
	}

	TEST_METHOD(ShiftCountsCoverTheFullMaskedDomainForEveryIntegralType)
	{
		VerifyShiftCounts<int8>(*TestRunner);
		VerifyShiftCounts<uint8>(*TestRunner);
		VerifyShiftCounts<int16>(*TestRunner);
		VerifyShiftCounts<uint16>(*TestRunner);
		VerifyShiftCounts<int32>(*TestRunner);
		VerifyShiftCounts<uint32>(*TestRunner);
		VerifyShiftCounts<int64>(*TestRunner);
		VerifyShiftCounts<uint64>(*TestRunner);
	}

	TEST_METHOD(DivideAndRemainderFailuresCoverSignedAndUnsignedTypes)
	{
		VerifyZeroFailures<int8>(*TestRunner);
		VerifyZeroFailures<uint8>(*TestRunner);
		VerifyZeroFailures<int16>(*TestRunner);
		VerifyZeroFailures<uint16>(*TestRunner);
		VerifyZeroFailures<int32>(*TestRunner);
		VerifyZeroFailures<uint32>(*TestRunner);
		VerifyZeroFailures<int64>(*TestRunner);
		VerifyZeroFailures<uint64>(*TestRunner);

		VerifySignedOverflowFailures<int8>(*TestRunner);
		VerifySignedOverflowFailures<int16>(*TestRunner);
		VerifySignedOverflowFailures<int32>(*TestRunner);
		VerifySignedOverflowFailures<int64>(*TestRunner);
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
