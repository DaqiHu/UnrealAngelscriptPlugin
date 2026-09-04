#include "CQTest.h"

#include "AngelscriptTestMacros.h"
#include "StaticJIT/AOT/UASFunctionDispatch/AngelscriptStaticJITAotUASFunctionDispatchTestBridge.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptStaticJITAotUASFunctionDispatchTests,
	"Angelscript.TestModule.StaticJIT.AOT.UASFunctionDispatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		AngelscriptStaticJITAotUASFunctionDispatchTestBridge::SetUpFixture();
	}

	AFTER_ALL()
	{
		AngelscriptStaticJITAotUASFunctionDispatchTestBridge::TearDownFixture();
	}

	TEST_METHOD(ExposesJitEntries)
	{
		ASSERT_THAT(IsTrue(
			AngelscriptStaticJITAotUASFunctionDispatchTestBridge::
				RunJitEntryAttachment(*TestRunner)));
	}

	TEST_METHOD(RuntimeCallEventUsesGeneratedJit)
	{
		ASSERT_THAT(IsTrue(
			AngelscriptStaticJITAotUASFunctionDispatchTestBridge::
				RunRuntimeCallEvent(*TestRunner)));
	}

	TEST_METHOD(RpcProcessEventPreservesGenericRoute)
	{
		ASSERT_THAT(IsTrue(
			AngelscriptStaticJITAotUASFunctionDispatchTestBridge::
				RunRpcProcessEventRoute(*TestRunner)));
	}

	TEST_METHOD(TypedProviderPreservesScriptOriginAndUsesParmsEntry)
	{
		ASSERT_THAT(IsTrue(
			AngelscriptStaticJITAotUASFunctionDispatchTestBridge::
				RunTypedIdentityAndParmsDispatch(*TestRunner)));
	}

	TEST_METHOD(TypedFinalScalarUsesExistingOptimizedRawWrapper)
	{
		ASSERT_THAT(IsTrue(
			AngelscriptStaticJITAotUASFunctionDispatchTestBridge::
				RunTypedFinalScalarRawDispatch(*TestRunner)));
	}

	TEST_METHOD(StaticWorldContextUsesGeneratedJit)
	{
		ASSERT_THAT(IsTrue(
			AngelscriptStaticJITAotUASFunctionDispatchTestBridge::
				RunStaticWorldContextRuntimeCallEvent(*TestRunner)));
	}

	TEST_METHOD(BytecodeFallbackWithGeneratedWorldContextAvoidsParmsEntry)
	{
		ASSERT_THAT(IsTrue(
			AngelscriptStaticJITAotUASFunctionDispatchTestBridge::
				RunBytecodeFallbackGeneratedWorldContext(*TestRunner)));
	}
};

#endif
