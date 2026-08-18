#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRUnsupportedLanguageBoundaryTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(FutureLanguageSyntaxRemainsRejectedWithOrWithoutCapture)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-LANGUAGE-BOUNDARY",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Isolation);

		struct FBoundaryCase
		{
			const ANSICHAR* Name;
			std::string Source;
		};

		const FBoundaryCase Cases[] =
		{
			{
				"try_catch",
				ASTEST_AS_ANSI(R"AS(
					int Entry()
					{
						try
						{
							return 42;
						}
						catch
						{
							return 0;
						}
					}
					)AS"),
			},
			{
				"try_without_catch",
				ASTEST_AS_ANSI(R"AS(
					int Entry()
					{
						try
						{
							return 42;
						}
					}
					)AS"),
			},
			{
				"catch_without_try",
				ASTEST_AS_ANSI(R"AS(
					int Entry()
					{
						catch
						{
							return 0;
						}
					}
					)AS"),
			},
			{
				"bare_rethrow",
				ASTEST_AS_ANSI(R"AS(
					int Entry()
					{
						throw;
						return 0;
					}
					)AS"),
			},
		};

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
			if (!Assertions.IsNotNull(
				ScriptEngine,
				TEXT("Language-boundary coverage should create a raw SDK Engine")))
			{
				continue;
			}
			(void)Assertions.IsTrue(
				ScriptEngine->SetTypedSemanticIRCapture(bCaptureEnabled),
				TEXT("Each case-owned Engine should accept its explicit capture policy"));

			for (const FBoundaryCase& Case : Cases)
			{
				const FString CaptureName = bCaptureEnabled
					? TEXT("CaptureOn")
					: TEXT("CaptureOff");
				const FString ModuleName = FString::Printf(
					TEXT("TypedSemanticIRLanguageBoundary_%s_%hs"),
					*CaptureName,
					Case.Name);
				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);

				Engine.ResetMessages();
				asIScriptModule* Module = nullptr;
				const int BuildResult = CompileNativeModule(
					ScriptEngine,
					ModuleNameUtf8.Get(),
					Case.Source.c_str(),
					Module);
				const FString Diagnostic = Engine.GetMessagesText();
				const FString Context = FString::Printf(
					TEXT("%hs with %s; result=%d diagnostic=%s"),
					Case.Name,
					*CaptureName,
					BuildResult,
					*Diagnostic);

				(void)Assertions.IsTrue(
					BuildResult < 0,
					*FString::Printf(
						TEXT("Current unsupported syntax should remain rejected: %s"),
						*Context));
				(void)Assertions.IsTrue(
					!Diagnostic.IsEmpty(),
					*FString::Printf(
						TEXT("Rejected syntax should retain a diagnostic: %s"),
						*Context));
				(void)Assertions.IsNull(
					GetNativeFunctionByExactDecl(Module, "int Entry()"),
					*FString::Printf(
						TEXT("Rejected syntax must not publish Entry or HIR: %s"),
						*Context));

				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
				(void)Assertions.IsNull(
					ScriptEngine->GetModule(
						ModuleNameUtf8.Get(),
						asGM_ONLY_IF_EXISTS),
					*FString::Printf(
						TEXT("Rejected module should discard cleanly: %s"),
						*Context));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
