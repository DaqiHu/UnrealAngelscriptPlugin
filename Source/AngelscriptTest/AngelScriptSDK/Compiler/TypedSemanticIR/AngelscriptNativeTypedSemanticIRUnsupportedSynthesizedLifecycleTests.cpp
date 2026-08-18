#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRUnsupportedSynthesizedLifecycleTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
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
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		if (Context->Prepare(Function) < 0
			|| Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		OutResult = static_cast<int32>(Context->GetReturnDWord());
		return true;
	}

public:
	TEST_METHOD(DefaultLifecycleFunctionsRecordStableUnsupportedDispositions)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-SYNTHESIZED-LIFECYCLE-DISPOSITION",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class FGeneratedLifecycleChild
			{
				int Value = 17;
			}

			class FGeneratedLifecycleCarrier
			{
				FGeneratedLifecycleChild Child =
					FGeneratedLifecycleChild();
			}

			int RunGeneratedLifecycle()
			{
				FGeneratedLifecycleCarrier Carrier =
					FGeneratedLifecycleCarrier();
				return Carrier.Child.Value;
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

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRSynthesizedLifecycleOff",
			ScriptSource);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRSynthesizedLifecycleOn",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid()));
		ASSERT_THAT(IsTrue(CaptureOnModule.IsValid()));
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		asCObjectType* const CaptureOffType = static_cast<asCObjectType*>(
			CaptureOffModule->GetTypeInfoByDecl("FGeneratedLifecycleCarrier"));
		asCObjectType* const CaptureOnType = static_cast<asCObjectType*>(
			CaptureOnModule->GetTypeInfoByDecl("FGeneratedLifecycleCarrier"));
		ASSERT_THAT(IsNotNull(CaptureOffType));
		ASSERT_THAT(IsNotNull(CaptureOnType));
		if (CaptureOffType == nullptr || CaptureOnType == nullptr)
		{
			return;
		}

		asCScriptEngine* const CaptureOffScriptEngine =
			static_cast<asCScriptEngine*>(CaptureOffEngine.Get());
		ASSERT_THAT(IsNotNull(CaptureOffScriptEngine));
		ASSERT_THAT(IsTrue(CaptureOnType->beh.construct > 0));
		ASSERT_THAT(IsTrue(CaptureOnType->beh.factory > 0));
		ASSERT_THAT(IsTrue(CaptureOnType->beh.destruct > 0,
			TEXT("The owned child should require a generated default destructor")));
		if (CaptureOffScriptEngine == nullptr
			|| CaptureOffType->beh.construct <= 0
			|| CaptureOffType->beh.factory <= 0
			|| CaptureOffType->beh.destruct <= 0
			|| CaptureOnType->beh.construct <= 0
			|| CaptureOnType->beh.factory <= 0
			|| CaptureOnType->beh.destruct <= 0)
		{
			return;
		}

		const int CaptureOffIds[] = {
			CaptureOffType->beh.construct,
			CaptureOffType->beh.factory,
			CaptureOffType->beh.destruct,
		};
		const int CaptureOnIds[] = {
			CaptureOnType->beh.construct,
			CaptureOnType->beh.factory,
			CaptureOnType->beh.destruct,
		};
		const TCHAR* const ExpectedDispositions[] = {
			TEXT("unsupported compiler-synthesized function: default-constructor"),
			TEXT("unsupported compiler-synthesized function: factory"),
			TEXT("unsupported compiler-synthesized function: default-destructor"),
		};

		for (int32 Index = 0; Index < 3; ++Index)
		{
			asCScriptFunction* const CaptureOffFunction =
				CaptureOffScriptEngine->scriptFunctions[CaptureOffIds[Index]];
			asCScriptFunction* const CaptureOnFunction =
				CaptureOnScriptEngine->scriptFunctions[CaptureOnIds[Index]];
			ASSERT_THAT(IsNotNull(CaptureOffFunction));
			ASSERT_THAT(IsNotNull(CaptureOnFunction));
			if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr)
			{
				continue;
			}

			ASSERT_THAT(IsNull(CaptureOnFunction->GetTypedSemanticFunction()));
			ASSERT_THAT(AreEqual(
				FString(ExpectedDispositions[Index]),
				FString(UTF8_TO_TCHAR(CaptureOnFunction
					->GetTypedSemanticCaptureDiagnostic().AddressOf()))));
			ASSERT_THAT(AreEqual(
				FString(),
				FString(UTF8_TO_TCHAR(CaptureOffFunction
					->GetTypedSemanticCaptureDiagnostic().AddressOf()))));

			asUINT BytecodeLength = 0;
			ASSERT_THAT(IsNotNull(CaptureOnFunction->GetByteCode(
				&BytecodeLength)));
			ASSERT_THAT(IsTrue(BytecodeLength > 0));
		}

		asIScriptFunction* const CaptureOffRun = GetNativeFunctionByDecl(
			CaptureOffModule, "int RunGeneratedLifecycle()");
		asIScriptFunction* const CaptureOnRun = GetNativeFunctionByDecl(
			CaptureOnModule, "int RunGeneratedLifecycle()");
		int32 CaptureOffResult = 0;
		int32 CaptureOnResult = 0;
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOffEngine.Get(), CaptureOffRun, CaptureOffResult)));
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOnEngine.Get(), CaptureOnRun, CaptureOnResult)));
		ASSERT_THAT(AreEqual(17, CaptureOffResult));
		ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult,
			TEXT("Synthesized lifecycle disposition must not change execution")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
