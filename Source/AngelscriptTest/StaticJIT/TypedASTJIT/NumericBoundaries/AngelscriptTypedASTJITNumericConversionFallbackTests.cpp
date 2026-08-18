#include "CQTest.h"

#include "../../../AngelScriptSDK/Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"
#include "StaticJIT/TypedASTJIT/AngelscriptTypedASTJITAnalyzer.h"

#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_module.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTypedASTJITNumericConversionFallbackTests,
	"Angelscript.TestModule.StaticJIT.TypedASTJIT.NumericBoundaries.ConversionFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptTypedASTJITAnalysis AnalyzeFunction(
		FAutomationTestBase& Test,
		asCModule& Module,
		const char* const FunctionName)
	{
		asCScriptFunction* const ScriptFunction =
			static_cast<asCScriptFunction*>(
				Module.GetFunctionByName(FunctionName));
		if (!Test.TestNotNull(
				TEXT("Numeric conversion function must exist"),
				ScriptFunction))
		{
			return {};
		}

		const asCTypedSemanticFunction* const HIR =
			ScriptFunction->GetTypedSemanticFunction();
		if (!Test.TestNotNull(
				TEXT("Numeric conversion function must publish HIR"),
				HIR))
		{
			return {};
		}

		FAngelscriptTypedASTJITFunctionShape Shape;
		Shape.SymbolName = FString::Printf(
			TEXT("ASJIT_%hs"), FunctionName);
		for (asUINT ParameterIndex = 0;
			ParameterIndex < ScriptFunction->parameterTypes.GetLength();
			++ParameterIndex)
		{
			Shape.ParameterTypes.Add(
				ScriptFunction->parameterTypes[ParameterIndex]);
		}
		Shape.ReturnType = ScriptFunction->returnType;
		Shape.InvocationKind = HIR->header.invocationKind;
		Shape.ReceiverKind = HIR->header.receiver.kind;
		return AnalyzeTypedASTJITFunction(*HIR, Shape);
	}

public:
	TEST_METHOD(FloatIntegerConversionsFailClosedWithTypedReason)
	{
		using namespace AngelscriptNativeTestSupport;

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asCScriptEngine* const ScriptEngine =
			static_cast<asCScriptEngine*>(Engine.Get());
		ASSERT_THAT(IsNotNull(ScriptEngine));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(ScriptEngine->SetTypedSemanticIRCapture(true)));

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int Float64ToSigned(float Value)
			{
				return int(Value);
			}

			uint Float32ToUnsigned(float32 Value)
			{
				return uint(Value);
			}

			float SignedToFloat64(int Value)
			{
				return float(Value);
			}

			float32 UnsignedToFloat32(uint Value)
			{
				return float32(Value);
			}
			)AS");
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedASTJITNumericConversionFallback",
			ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid()));
		if (!Module.IsValid())
		{
			return;
		}

		asCModule* const ScriptModule =
			static_cast<asCModule*>(Module.Get());
		ASSERT_THAT(IsNotNull(ScriptModule));
		if (ScriptModule == nullptr)
		{
			return;
		}

		const char* const FunctionNames[] = {
			"Float64ToSigned",
			"Float32ToUnsigned",
			"SignedToFloat64",
			"UnsignedToFloat32",
		};
		for (const char* const FunctionName : FunctionNames)
		{
			const FAngelscriptTypedASTJITAnalysis Analysis =
				AnalyzeFunction(*TestRunner, *ScriptModule, FunctionName);
			ASSERT_THAT(IsFalse(Analysis.bEligible,
				FString::Printf(TEXT("%hs must remain outside the reviewed numeric-conversion slice"), FunctionName)));
			ASSERT_THAT(AreEqual(
				EAngelscriptTypedASTJITFallbackReason::
					NonPortableNumericConversion,
				Analysis.FailureReason,
				FString::Printf(TEXT("%hs must expose the typed non-portable conversion reason"), FunctionName)));
			ASSERT_THAT(IsFalse(Analysis.FailureDetail.IsEmpty(),
				FString::Printf(TEXT("%hs must retain a deterministic conversion detail"), FunctionName)));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
