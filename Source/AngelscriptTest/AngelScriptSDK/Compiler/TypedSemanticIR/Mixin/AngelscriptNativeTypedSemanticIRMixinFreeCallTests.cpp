#include "../../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRMixinFreeCallTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.Mixin.FreeCall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(FreeCallRemainsRejectedWithCaptureOffAndOn)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-MIXIN-FREE-CALL-REJECTION",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FMixinFreeValue
			{
				int Value = 0;
			}

			mixin int EvaluateFreeMixin(FMixinFreeValue& Self, int Delta)
			{
				return Self.Value + Delta;
			}

			int Entry()
			{
				FMixinFreeValue Value;
				return EvaluateFreeMixin(Value, 3);
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
		ASSERT_THAT(IsTrue(
			CaptureOnScriptEngine->SetTypedSemanticIRCapture(true)));

		asIScriptModule* CaptureOffModule = nullptr;
		asIScriptModule* CaptureOnModule = nullptr;
		CaptureOffEngine.ResetMessages();
		CaptureOnEngine.ResetMessages();
		const int CaptureOffResult = CompileNativeModule(
			CaptureOffEngine.Get(),
			"TypedSemanticIRMixinFreeCallOff",
			ScriptSource.c_str(),
			CaptureOffModule);
		const int CaptureOnResult = CompileNativeModule(
			CaptureOnEngine.Get(),
			"TypedSemanticIRMixinFreeCallOn",
			ScriptSource.c_str(),
			CaptureOnModule);
		ON_SCOPE_EXIT
		{
			if (CaptureOnEngine.Get()->GetModule(
				"TypedSemanticIRMixinFreeCallOn",
				asGM_ONLY_IF_EXISTS) != nullptr)
			{
				CaptureOnEngine.Get()->DiscardModule(
					"TypedSemanticIRMixinFreeCallOn");
			}
			if (CaptureOffEngine.Get()->GetModule(
				"TypedSemanticIRMixinFreeCallOff",
				asGM_ONLY_IF_EXISTS) != nullptr)
			{
				CaptureOffEngine.Get()->DiscardModule(
					"TypedSemanticIRMixinFreeCallOff");
			}
		};

		ASSERT_THAT(IsTrue(CaptureOffResult < 0));
		ASSERT_THAT(IsTrue(CaptureOnResult < 0));
		const FString CaptureOffDiagnostics =
			CaptureOffEngine.GetMessagesText();
		const FString CaptureOnDiagnostics =
			CaptureOnEngine.GetMessagesText();
		ASSERT_THAT(IsTrue(
			CaptureOffDiagnostics.Contains(TEXT("No matching signatures"))
				&& CaptureOffDiagnostics.Contains(TEXT("EvaluateFreeMixin")),
			TEXT("The capture-off compiler must retain the current mixin free-call diagnostic")));
		ASSERT_THAT(IsTrue(
			CaptureOnDiagnostics.Contains(TEXT("No matching signatures"))
				&& CaptureOnDiagnostics.Contains(TEXT("EvaluateFreeMixin")),
			TEXT("HIR capture must not make mixin functions callable as ordinary globals")));
		ASSERT_THAT(IsTrue(
			CaptureOffModule == nullptr
				|| CaptureOffModule->GetFunctionByDecl("int Entry()") == nullptr));
		ASSERT_THAT(IsTrue(
			CaptureOnModule == nullptr
				|| CaptureOnModule->GetFunctionByDecl("int Entry()") == nullptr,
			TEXT("Rejected source must not publish an Entry function or partial HIR")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
