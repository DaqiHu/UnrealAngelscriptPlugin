#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRUnsupportedSynthesizedLambdaTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(FutureLambdaSyntaxRemainsRejectedWithOrWithoutCapture)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-SYNTHESIZED-LAMBDA-BOUNDARY",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			funcdef int UnaryOperation(int);

			int Entry()
			{
				UnaryOperation@ AddTwo = function(int Value)
				{
					return Value + 2;
				};
				return AddTwo(40);
			}
			)AS");

		FNoDiscardAsserter Assertions(*TestRunner);
		for (const bool bCaptureEnabled : { false, true })
		{
			FNativeTestEngine Engine;
			Engine.Create(*TestRunner);
			ON_SCOPE_EXIT
			{
				Engine.Destroy();
			};

			asCScriptEngine* const ScriptEngine =
				static_cast<asCScriptEngine*>(Engine.Get());
			if (!Assertions.IsNotNull(ScriptEngine))
			{
				continue;
			}
			(void)Assertions.IsTrue(
				ScriptEngine->SetTypedSemanticIRCapture(bCaptureEnabled));

			const char* const ModuleName = bCaptureEnabled
				? "TypedSemanticIRSynthesizedLambdaOn"
				: "TypedSemanticIRSynthesizedLambdaOff";
			Engine.ResetMessages();
			asIScriptModule* Module = nullptr;
			const int BuildResult = CompileNativeModule(
				ScriptEngine,
				ModuleName,
				ScriptSource.c_str(),
				Module);
			const FString Diagnostic = Engine.GetMessagesText();

			(void)Assertions.IsTrue(BuildResult < 0,
				TEXT("The maintained source surface must reject future lambda syntax"));
			(void)Assertions.IsTrue(!Diagnostic.IsEmpty(),
				TEXT("Rejected lambda syntax should retain a diagnostic"));
			(void)Assertions.IsNull(
				GetNativeFunctionByExactDecl(Module, "int Entry()"),
				TEXT("Rejected lambda syntax must publish neither Entry nor HIR"));

			ScriptEngine->DiscardModule(ModuleName);
			(void)Assertions.IsNull(
				ScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
				TEXT("The rejected lambda module should discard cleanly"));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
