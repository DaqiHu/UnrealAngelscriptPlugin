#include "../../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptNativeTypedSemanticIRExternalImplicitThisTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRExternalImplicitThisNullTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.ExternalImplicitThis.Null",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(NullHandleBehaviorRemainsTheBytecodeExceptionPath)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIRExternalImplicitThisTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-EXTERNAL-IMPLICIT-THIS-NULL",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class FExternalNullCarrier
			{
				int Value = 31;
			}

			int ReadExternalNull(FExternalNullCarrier Self) external_implicit_this
			{
				return this.Value;
			}

			int RunExternalNull()
			{
				FExternalNullCarrier Receiver = nullptr;
				return ReadExternalNull(Receiver);
			}
			)AS");

		FNativeTestEngine CaptureOffEngine;
		FNativeTestEngine CaptureOnEngine;
		CaptureOffEngine.Create(*TestRunner);
		CaptureOnEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			CaptureOnEngine.Destroy();
			CaptureOffEngine.Destroy();
		};
		asCScriptEngine* const CaptureOnScriptEngine =
			static_cast<asCScriptEngine*>(CaptureOnEngine.Get());
		ASSERT_THAT(IsNotNull(CaptureOnScriptEngine));
		if (CaptureOnScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(CaptureOnScriptEngine->SetTypedSemanticIRCapture(true)));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRExternalNullOff",
			ScriptSource);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRExternalNullOn",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid()));
		ASSERT_THAT(IsTrue(CaptureOnModule.IsValid()));
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		const char* const ReadDeclaration =
			"int ReadExternalNull(FExternalNullCarrier Self)";
		asCScriptFunction* const CaptureOffRead =
			static_cast<asCScriptFunction*>(GetNativeFunctionByExactDecl(
				CaptureOffModule,
				ReadDeclaration));
		asCScriptFunction* const CaptureOnRead =
			static_cast<asCScriptFunction*>(GetNativeFunctionByExactDecl(
				CaptureOnModule,
				ReadDeclaration));
		asIScriptFunction* const CaptureOffRun = GetNativeFunctionByExactDecl(
			CaptureOffModule,
			"int RunExternalNull()");
		asIScriptFunction* const CaptureOnRun = GetNativeFunctionByExactDecl(
			CaptureOnModule,
			"int RunExternalNull()");
		ASSERT_THAT(IsNotNull(CaptureOffRead));
		ASSERT_THAT(IsNotNull(CaptureOnRead));
		ASSERT_THAT(IsNotNull(CaptureOffRun));
		ASSERT_THAT(IsNotNull(CaptureOnRun));
		if (CaptureOffRead == nullptr || CaptureOnRead == nullptr
			|| CaptureOffRun == nullptr || CaptureOnRun == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			CopyNormalizedBytecode(CaptureOffRead),
			CopyNormalizedBytecode(CaptureOnRead),
			TEXT("Capture must not replace the VM null-dereference instructions after normalizing only Engine-local pointer operands")));
		const FExecutionObservation CaptureOffExecution = ExecuteGlobalInt(
			CaptureOffEngine.Get(),
			CaptureOffRun);
		const FExecutionObservation CaptureOnExecution = ExecuteGlobalInt(
			CaptureOnEngine.Get(),
			CaptureOnRun);
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, CaptureOffExecution.State));
		ASSERT_THAT(AreEqual(asEXECUTION_EXCEPTION, CaptureOnExecution.State));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Null pointer access")),
			CaptureOffExecution.Exception));
		ASSERT_THAT(AreEqual(
			CaptureOffExecution.Exception,
			CaptureOnExecution.Exception));
		ASSERT_THAT(IsNull(CaptureOffRead->GetTypedSemanticFunction()));
		ASSERT_THAT(IsNotNull(CaptureOnRead->GetTypedSemanticFunction(),
			TEXT("Null handling remains bytecode behavior, not an HIR capture failure")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
