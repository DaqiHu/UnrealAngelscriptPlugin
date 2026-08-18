#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRUnsupportedSynthesizedAccessorTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(RemovedVirtualPropertyAccessorRemainsRejectedWithOrWithoutCapture)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-SYNTHESIZED-ACCESSOR-BOUNDARY",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class FRemovedVirtualProperty
			{
				int Stored = 42;

				int Published
				{
					get
					{
						return Stored;
					}
				}
			}
			)AS");
		const FString ExpectedDiagnostic = TEXT(
			"Virtual property syntax has been removed. Use explicit GetX/SetX methods instead");

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
				? "TypedSemanticIRSynthesizedAccessorOn"
				: "TypedSemanticIRSynthesizedAccessorOff";
			Engine.ResetMessages();
			asIScriptModule* Module = nullptr;
			const int BuildResult = CompileNativeModule(
				ScriptEngine,
				ModuleName,
				ScriptSource.c_str(),
				Module);
			const FString Diagnostic = Engine.GetMessagesText();

			(void)Assertions.IsTrue(BuildResult < 0,
				TEXT("Removed virtual-property syntax must remain rejected"));
			(void)Assertions.IsTrue(Diagnostic.Contains(ExpectedDiagnostic),
				*FString::Printf(
					TEXT("Accessor rejection should retain its stable reason: %s"),
					*Diagnostic));
			(void)Assertions.IsNull(
				Module != nullptr
					? Module->GetTypeInfoByDecl("FRemovedVirtualProperty")
					: nullptr,
				TEXT("Rejected accessor syntax must publish no script type or HIR"));

			ScriptEngine->DiscardModule(ModuleName);
			(void)Assertions.IsNull(
				ScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS),
				TEXT("The rejected accessor module should discard cleanly"));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
