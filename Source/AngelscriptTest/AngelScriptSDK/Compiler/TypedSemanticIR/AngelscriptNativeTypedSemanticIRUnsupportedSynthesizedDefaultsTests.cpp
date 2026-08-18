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

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRUnsupportedSynthesizedDefaultsTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FDefaultsExecution
	{
		int32 Before = 0;
		int32 After = 0;
	};

	static bool ExecuteInitDefaults(
		asIScriptEngine* ScriptEngine,
		asITypeInfo* Type,
		asIScriptFunction* InitDefaults,
		FDefaultsExecution& OutExecution)
	{
		if (ScriptEngine == nullptr || Type == nullptr || InitDefaults == nullptr)
		{
			return false;
		}

		void* const ObjectPointer = ScriptEngine->CreateScriptObject(Type);
		if (ObjectPointer == nullptr)
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			ScriptEngine->ReleaseScriptObject(ObjectPointer, Type);
		};

		asIScriptObject* const Object =
			static_cast<asIScriptObject*>(ObjectPointer);
		void* const PropertyAddress = Object->GetAddressOfProperty(0);
		if (PropertyAddress == nullptr)
		{
			return false;
		}
		OutExecution.Before = *static_cast<int32*>(PropertyAddress);

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		if (Context == nullptr)
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		if (Context->Prepare(InitDefaults) < 0
			|| Context->SetObject(ObjectPointer) < 0
			|| Context->Execute() != asEXECUTION_FINISHED)
		{
			return false;
		}
		OutExecution.After = *static_cast<int32*>(PropertyAddress);
		return true;
	}

public:
	TEST_METHOD(InitDefaultsRecordsStableUnsupportedDispositionWithoutChangingExecution)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-SYNTHESIZED-INIT-DEFAULTS-DISPOSITION",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class FGeneratedDefaults
			{
				int Value = 5;

				default Value = 37;
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

		asCScriptEngine* const CaptureOffScriptEngine =
			static_cast<asCScriptEngine*>(CaptureOffEngine.Get());
		asCScriptEngine* const CaptureOnScriptEngine =
			static_cast<asCScriptEngine*>(CaptureOnEngine.Get());
		ASSERT_THAT(IsNotNull(CaptureOffScriptEngine));
		ASSERT_THAT(IsNotNull(CaptureOnScriptEngine));
		if (CaptureOffScriptEngine == nullptr || CaptureOnScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			CaptureOnScriptEngine->SetTypedSemanticIRCapture(true)));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRSynthesizedDefaultsOff",
			ScriptSource);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRSynthesizedDefaultsOn",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid()));
		ASSERT_THAT(IsTrue(CaptureOnModule.IsValid()));
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		asCObjectType* const CaptureOffType = static_cast<asCObjectType*>(
			CaptureOffModule->GetTypeInfoByDecl("FGeneratedDefaults"));
		asCObjectType* const CaptureOnType = static_cast<asCObjectType*>(
			CaptureOnModule->GetTypeInfoByDecl("FGeneratedDefaults"));
		ASSERT_THAT(IsNotNull(CaptureOffType));
		ASSERT_THAT(IsNotNull(CaptureOnType));
		if (CaptureOffType == nullptr || CaptureOnType == nullptr)
		{
			return;
		}

		asCScriptFunction* const CaptureOffInit =
			static_cast<asCScriptFunction*>(
				CaptureOffType->GetMethodByDecl("void __InitDefaults()"));
		asCScriptFunction* const CaptureOnInit =
			static_cast<asCScriptFunction*>(
				CaptureOnType->GetMethodByDecl("void __InitDefaults()"));
		ASSERT_THAT(IsNotNull(CaptureOffInit));
		ASSERT_THAT(IsNotNull(CaptureOnInit));
		if (CaptureOffInit == nullptr || CaptureOnInit == nullptr)
		{
			return;
		}

		asUINT CaptureOffBytecodeLength = 0;
		asUINT CaptureOnBytecodeLength = 0;
		const asDWORD* const CaptureOffBytecode =
			CaptureOffInit->GetByteCode(&CaptureOffBytecodeLength);
		const asDWORD* const CaptureOnBytecode =
			CaptureOnInit->GetByteCode(&CaptureOnBytecodeLength);
		ASSERT_THAT(IsNotNull(CaptureOffBytecode));
		ASSERT_THAT(IsNotNull(CaptureOnBytecode));
		ASSERT_THAT(AreEqual(CaptureOffBytecodeLength, CaptureOnBytecodeLength));
		ASSERT_THAT(IsTrue(CaptureOffBytecodeLength > 0));
		if (CaptureOffBytecode != nullptr && CaptureOnBytecode != nullptr)
		{
			ASSERT_THAT(IsTrue(FMemory::Memcmp(
				CaptureOffBytecode,
				CaptureOnBytecode,
				CaptureOffBytecodeLength * sizeof(asDWORD)) == 0,
				TEXT("HIR capture must not alter __InitDefaults bytecode")));
		}

		FDefaultsExecution CaptureOffExecution;
		FDefaultsExecution CaptureOnExecution;
		ASSERT_THAT(IsTrue(ExecuteInitDefaults(
			CaptureOffScriptEngine,
			CaptureOffType,
			CaptureOffInit,
			CaptureOffExecution)));
		ASSERT_THAT(IsTrue(ExecuteInitDefaults(
			CaptureOnScriptEngine,
			CaptureOnType,
			CaptureOnInit,
			CaptureOnExecution)));
		ASSERT_THAT(AreEqual(5, CaptureOffExecution.Before));
		ASSERT_THAT(AreEqual(37, CaptureOffExecution.After));
		ASSERT_THAT(AreEqual(
			CaptureOffExecution.Before,
			CaptureOnExecution.Before));
		ASSERT_THAT(AreEqual(
			CaptureOffExecution.After,
			CaptureOnExecution.After,
			TEXT("Unsupported HIR metadata must not change default execution")));

		ASSERT_THAT(IsNull(CaptureOffInit->GetTypedSemanticFunction()));
		ASSERT_THAT(IsNull(CaptureOnInit->GetTypedSemanticFunction()));
		ASSERT_THAT(AreEqual(
			FString(),
			FString(UTF8_TO_TCHAR(CaptureOffInit
				->GetTypedSemanticCaptureDiagnostic().AddressOf()))));
		ASSERT_THAT(AreEqual(
			FString(TEXT(
				"unsupported compiler-synthesized function: init-defaults")),
			FString(UTF8_TO_TCHAR(CaptureOnInit
				->GetTypedSemanticCaptureDiagnostic().AddressOf()))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
