#include "../../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRExternalImplicitThisMalformedTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.ExternalImplicitThis.Malformed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(MalformedReceiverShapesKeepFrontendPolicyAndPublishNoHIR)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-EXTERNAL-IMPLICIT-THIS-MALFORMED",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		struct FMalformedCase
		{
			const ANSICHAR* Name;
			std::string Source;
			const ANSICHAR* Declaration;
			asUINT ParameterCount;
		};
		const FMalformedCase Cases[] =
		{
			{
				"missing_parameter_zero",
				ASTEST_AS_ANSI(R"AS(
					int MissingExternalReceiver() external_implicit_this
					{
						return 41;
					}
					)AS"),
				"int MissingExternalReceiver()",
				0,
			},
			{
				"primitive_parameter_zero",
				ASTEST_AS_ANSI(R"AS(
					int PrimitiveExternalReceiver(int Value) external_implicit_this
					{
						return Value;
					}
					)AS"),
				"int PrimitiveExternalReceiver(const int)",
				1,
			},
		};

		struct FBuildObservation
		{
			int Result = asERROR;
			FString Diagnostic;
		};
		TArray<FBuildObservation> CaptureOffObservations;
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

			for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(Cases); ++CaseIndex)
			{
				const FMalformedCase& Case = Cases[CaseIndex];
				const FString ModuleName = FString::Printf(
					TEXT("TypedSemanticIRExternalMalformed_%s_%hs"),
					bCaptureEnabled ? TEXT("On") : TEXT("Off"),
					Case.Name);
				const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);

				Engine.ResetMessages();
				asIScriptModule* Module = nullptr;
				const int BuildResult = CompileNativeModule(
					ScriptEngine,
					ModuleNameUtf8.Get(),
					Case.Source.c_str(),
					Module);
				const FString FrontendDiagnostic = Engine.GetMessagesText();
				asCScriptFunction* const Function =
					static_cast<asCScriptFunction*>(GetNativeFunctionByExactDecl(
						Module,
						Case.Declaration));
				const FString Context = FString::Printf(
					TEXT("%hs capture=%s result=%d frontend=%s"),
					Case.Name,
					bCaptureEnabled ? TEXT("on") : TEXT("off"),
					BuildResult,
					*FrontendDiagnostic);

				(void)Assertions.IsTrue(BuildResult >= 0,
					*FString::Printf(
						TEXT("HIR validation must not invent a frontend error: %s"),
						*Context));
				(void)Assertions.IsTrue(FrontendDiagnostic.IsEmpty(),
					*FString::Printf(
						TEXT("Current frontend policy accepts the declaration silently: %s"),
						*Context));
				(void)Assertions.IsNotNull(Function,
					*FString::Printf(
						TEXT("The accepted malformed declaration remains a VM function: %s"),
						*Context));
				if (Function != nullptr)
				{
					(void)Assertions.AreEqual(
						Case.ParameterCount,
						Function->GetParamCount(),
						*Context);
					(void)Assertions.IsNull(Function->GetObjectType(), *Context);
					(void)Assertions.IsNull(
						Function->GetTypedSemanticFunction(),
						*FString::Printf(
							TEXT("Invalid receiver metadata must publish no HIR or TypedASTJIT input: %s"),
							*Context));
					if (bCaptureEnabled)
					{
						const FString CaptureDiagnostic = UTF8_TO_TCHAR(
							Function->GetTypedSemanticCaptureDiagnostic().AddressOf());
						(void)Assertions.IsTrue(
							CaptureDiagnostic.Contains(TEXT("InvalidEffectiveReceiver")),
							*FString::Printf(
								TEXT("Malformed receiver fallback needs a stable verifier code: %s captureDiagnostic=%s"),
								*Context,
								*CaptureDiagnostic));
					}
				}

				FBuildObservation Observation;
				Observation.Result = BuildResult;
				Observation.Diagnostic = FrontendDiagnostic;
				if (!bCaptureEnabled)
				{
					CaptureOffObservations.Add(Observation);
				}
				else if (CaptureOffObservations.IsValidIndex(CaseIndex))
				{
					(void)Assertions.AreEqual(
						CaptureOffObservations[CaseIndex].Result,
						Observation.Result,
						*Context);
					(void)Assertions.AreEqual(
						CaptureOffObservations[CaseIndex].Diagnostic,
						Observation.Diagnostic,
						*Context);
				}

				ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
