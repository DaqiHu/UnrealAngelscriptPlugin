#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRPublicationTransactionTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	// Reserved only by the maintained-fork compiler fault seam. A non-null value
	// invalidates the provisional HIR immediately before its real verification;
	// it never changes finalized bytecode or the public AngelScript ABI.
	static constexpr asPWORD InvalidProvisionalHIRUserDataSlot =
		static_cast<asPWORD>(0x54534952494E564Cull);

	static bool ExecuteGlobalInt(
		asIScriptEngine* ScriptEngine,
		asIScriptFunction* Function,
		int32& OutResult)
	{
		if (ScriptEngine == nullptr || Function == nullptr)
		{
			return false;
		}

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		if (Context == nullptr)
		{
			return false;
		}
		ON_SCOPE_EXIT { Context->Release(); };

		if (Context->Prepare(Function) < 0
			|| Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		OutResult = static_cast<int32>(Context->GetReturnDWord());
		return true;
	}

public:
	TEST_METHOD(VerifierInvalidProvisionalHIRKeepsExecutableBytecodeWithoutPublishingHIR)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-PUBLICATION-TRANSACTION",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int ProvisionalHIRFallback()
			{
				int Value = 40;
				return Value + 2;
			}
			)AS");

		FNativeTestEngine CaptureOffEngine;
		FNativeTestEngine InvalidCaptureEngine;
		CaptureOffEngine.Create(*TestRunner);
		InvalidCaptureEngine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			if (InvalidCaptureEngine.Get() != nullptr)
			{
				InvalidCaptureEngine.Get()->SetUserData(
					nullptr,
					InvalidProvisionalHIRUserDataSlot);
			}
			InvalidCaptureEngine.Destroy();
			CaptureOffEngine.Destroy();
		};

		asCScriptEngine* const InvalidCaptureScriptEngine =
			static_cast<asCScriptEngine*>(InvalidCaptureEngine.Get());
		ASSERT_THAT(IsNotNull(InvalidCaptureScriptEngine));
		if (InvalidCaptureScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			InvalidCaptureScriptEngine->SetTypedSemanticIRCapture(true)));
		uint8 FaultSentinel = 1;
		ASSERT_THAT(IsNull(InvalidCaptureScriptEngine->SetUserData(
			&FaultSentinel,
			InvalidProvisionalHIRUserDataSlot)));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRPublicationCaptureOff",
			ScriptSource);
		FScopedNativeModule InvalidCaptureModule(
			*TestRunner,
			InvalidCaptureEngine,
			"TypedSemanticIRPublicationInvalidCapture",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid(),
			TEXT("The capture-off bytecode oracle must compile")));
		ASSERT_THAT(IsTrue(InvalidCaptureModule.IsValid(),
			TEXT("A verifier-invalid sidecar must not fail valid script compilation")));
		if (!CaptureOffModule.IsValid() || !InvalidCaptureModule.IsValid())
		{
			return;
		}

		asCScriptFunction* const CaptureOffFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOffModule,
				"int ProvisionalHIRFallback()"));
		asCScriptFunction* const InvalidCaptureFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				InvalidCaptureModule,
				"int ProvisionalHIRFallback()"));
		ASSERT_THAT(IsNotNull(CaptureOffFunction));
		ASSERT_THAT(IsNotNull(InvalidCaptureFunction));
		if (CaptureOffFunction == nullptr || InvalidCaptureFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsNull(CaptureOffFunction->GetTypedSemanticFunction()));
		ASSERT_THAT(IsNull(InvalidCaptureFunction->GetTypedSemanticFunction(),
			TEXT("Verifier-invalid provisional HIR must never enter ScriptFunctionData")));
		const FString InvalidDiagnostic = UTF8_TO_TCHAR(
			InvalidCaptureFunction->GetTypedSemanticCaptureDiagnostic().AddressOf());
		ASSERT_THAT(IsTrue(
			InvalidDiagnostic.Contains(TEXT("semantic verification failed")),
			*FString::Printf(
				TEXT("The fallback must retain a verifier diagnostic, got: %s"),
				*InvalidDiagnostic)));
		ASSERT_THAT(IsTrue(
			InvalidDiagnostic.Contains(TEXT("Function root is missing")),
			TEXT("The injected fault should fail through the real MissingRoot verifier path")));

		const TArray<asDWORD> CaptureOffBytecode =
			CopyNativeBytecode(CaptureOffFunction);
		const TArray<asDWORD> InvalidCaptureBytecode =
			CopyNativeBytecode(InvalidCaptureFunction);
		ASSERT_THAT(IsTrue(!CaptureOffBytecode.IsEmpty()));
		ASSERT_THAT(AreEqual(
			CaptureOffBytecode,
			InvalidCaptureBytecode,
			TEXT("A sidecar verifier failure must not alter finalized bytecode")));

		int32 CaptureOffResult = 0;
		int32 InvalidCaptureResult = 0;
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOffEngine.Get(),
			CaptureOffFunction,
			CaptureOffResult)));
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			InvalidCaptureEngine.Get(),
			InvalidCaptureFunction,
			InvalidCaptureResult)));
		ASSERT_THAT(AreEqual(42, CaptureOffResult));
		ASSERT_THAT(AreEqual(CaptureOffResult, InvalidCaptureResult));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
