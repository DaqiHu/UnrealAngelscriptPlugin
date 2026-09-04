#include "CQTest.h"

#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITScalarOps.h"
#include "Testing/AngelscriptScriptTestRunner.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITScalarOpsTests,
	"Angelscript.TestModule.StaticJIT.ScalarOps.TypedASTJIT",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(SignedArithmeticWrapsInTheMatchingUnsignedBitDomain)
	{
		using namespace AngelscriptTypedASTJIT;

		ASSERT_THAT(AreEqual(MIN_int8,
			WrapAdd<int8>(MAX_int8, int8{1})));
		ASSERT_THAT(AreEqual(MAX_int16,
			WrapSubtract<int16>(MIN_int16, int16{1})));
		ASSERT_THAT(AreEqual(int32{-2},
			WrapMultiply<int32>(MAX_int32, int32{2})));
		ASSERT_THAT(AreEqual(MIN_int64,
			WrapNegate<int64>(MIN_int64)));
	}

	TEST_METHOD(ShiftCountsAreMaskedAndArithmeticRightShiftUsesExplicitSignFill)
	{
		using namespace AngelscriptTypedASTJIT;

		ASSERT_THAT(AreEqual(MIN_int32,
			ShiftLeft<int32>(int32{1}, int32{-1})));
		ASSERT_THAT(AreEqual(int32{1},
			ShiftLeft<int32>(int32{1}, int32{32})));
		ASSERT_THAT(AreEqual(MAX_int32,
			LogicalShiftRight<int32>(int32{-1}, int32{1})));
		ASSERT_THAT(AreEqual(int32{-1073741824},
			ArithmeticShiftRight<int32>(MIN_int32, int32{1})));
		ASSERT_THAT(AreEqual(int64{-1},
			ArithmeticShiftRight<int64>(int64{-1}, int64{64})));
		ASSERT_THAT(AreEqual(int64{1} << 3,
			ShiftLeft<int64>(int64{1}, int64{131})));
	}

	TEST_METHOD(CheckedDivisionAndRemainderMatchTheMaintainedVmFailureOrder)
	{
		using namespace AngelscriptTypedASTJIT;

		{
			FScriptExecution NormalExecution(asCThreadManager::GetLocalData());
			ASSERT_THAT(AreEqual(int32{4},
				CheckedDivide<int32>(
					NormalExecution, int32{12}, int32{3})));
			ASSERT_THAT(AreEqual(uint64{2},
				CheckedRemainder<uint64>(
					NormalExecution, uint64{14}, uint64{4})));
			ASSERT_THAT(IsFalse(NormalExecution.bExceptionThrown));
		}
		{
			FScriptExecution DivisionByZero(asCThreadManager::GetLocalData());
			int32 DivisionResult = 99;
			{
				FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
				DivisionResult = CheckedDivide<int32>(
					DivisionByZero, int32{12}, int32{0});
			}
			ASSERT_THAT(AreEqual(int32{0}, DivisionResult));
			ASSERT_THAT(IsTrue(DivisionByZero.bExceptionThrown));
			ASSERT_THAT(IsNotNull(DivisionByZero.FailureRecord));
			ASSERT_THAT(AreEqual(
				FString(TEXT("Divide by zero")),
				FString(UTF8_TO_TCHAR(
					DivisionByZero.FailureRecord->Message.AddressOf()))));
		}
		{
			FScriptExecution RemainderOverflow(asCThreadManager::GetLocalData());
			int64 RemainderResult = 99;
			{
				FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
				RemainderResult = CheckedRemainder<int64>(
					RemainderOverflow, MIN_int64, int64{-1});
			}
			ASSERT_THAT(AreEqual(int64{0}, RemainderResult));
			ASSERT_THAT(IsTrue(RemainderOverflow.bExceptionThrown));
			ASSERT_THAT(IsNotNull(RemainderOverflow.FailureRecord));
			const FString OverflowMessage = UTF8_TO_TCHAR(
				RemainderOverflow.FailureRecord->Message.AddressOf());
			ASSERT_THAT(AreEqual(28, OverflowMessage.Len()));
			ASSERT_THAT(IsTrue(OverflowMessage.Equals(
				TEXT("Overflow in integer division"),
				ESearchCase::CaseSensitive)));
		}
	}

	TEST_METHOD(CheckedIntegralFailuresKeepTheFirstFailureRecord)
	{
		using namespace AngelscriptTypedASTJIT;

		FScriptExecution Execution(asCThreadManager::GetLocalData());
		{
			FAngelscriptScriptTestCallbackScope SuppressExpectedExceptionLog(true);
			CheckedDivide<int32>(Execution, int32{7}, int32{0});
			CheckedRemainder<int32>(Execution, MIN_int32, int32{-1});
		}

		ASSERT_THAT(IsTrue(Execution.bExceptionThrown));
		ASSERT_THAT(IsNotNull(Execution.FailureRecord));
		ASSERT_THAT(IsTrue(Execution.FailureRecord->bHasFailure));
		ASSERT_THAT(IsTrue(Execution.FailureRecord->bReported));
		ASSERT_THAT(IsFalse(Execution.FailureRecord->bAdopted));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Divide by zero")),
			FString(UTF8_TO_TCHAR(
				Execution.FailureRecord->Message.AddressOf()))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("generated-native-call")),
			FString(UTF8_TO_TCHAR(
				Execution.FailureRecord->RouteOrigin.AddressOf()))));
		ASSERT_THAT(AreEqual(
			FString(TEXT("static-jit")),
			FString(UTF8_TO_TCHAR(
				Execution.FailureRecord->BackendOrigin.AddressOf()))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
