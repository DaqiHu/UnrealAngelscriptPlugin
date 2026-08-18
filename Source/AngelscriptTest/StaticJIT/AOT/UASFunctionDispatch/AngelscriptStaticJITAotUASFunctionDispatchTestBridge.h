#pragma once

class FAutomationTestBase;

namespace AngelscriptStaticJITAotUASFunctionDispatchTestBridge
{
	void SetUpFixture();
	void TearDownFixture();

	bool RunJitEntryAttachment(FAutomationTestBase& Test);
	bool RunRuntimeCallEvent(FAutomationTestBase& Test);
	bool RunRpcProcessEventRoute(FAutomationTestBase& Test);
	bool RunTypedIdentityAndParmsDispatch(FAutomationTestBase& Test);
	bool RunTypedFinalScalarRawDispatch(FAutomationTestBase& Test);
	bool RunStaticWorldContextRuntimeCallEvent(FAutomationTestBase& Test);
	bool RunBytecodeFallbackGeneratedWorldContext(FAutomationTestBase& Test);
}
