#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_typed_semantic_ir.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRUnsupportedSynthesizedFunctionTests,
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
	TEST_METHOD(GeneratedTraitPublishesOneDeterministicUnsupportedDisposition)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-UNSUPPORTED-SYNTHESIZED-FUNCTION",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int RunGeneratedDisposition() __generated
			{
				return 21 + 21;
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
			CaptureOnScriptEngine->SetTypedSemanticIRCapture(true),
			TEXT("The case-owned capture Engine should enable HIR before build")));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRSynthesizedFunctionOff",
			ScriptSource);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRSynthesizedFunctionOn",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid(),
			TEXT("The generated trait must remain valid with capture off")));
		ASSERT_THAT(IsTrue(CaptureOnModule.IsValid(),
			TEXT("The generated trait must remain valid with capture on")));
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		asCScriptFunction* const CaptureOffFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOffModule,
				"int RunGeneratedDisposition()"));
		asCScriptFunction* const CaptureOnFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOnModule,
				"int RunGeneratedDisposition()"));
		ASSERT_THAT(IsNotNull(CaptureOffFunction));
		ASSERT_THAT(IsNotNull(CaptureOnFunction));
		if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(CaptureOffFunction->traits.GetTrait(
			asTRAIT_GENERATED_FUNCTION)));
		ASSERT_THAT(IsTrue(CaptureOnFunction->traits.GetTrait(
			asTRAIT_GENERATED_FUNCTION)));
		ASSERT_THAT(IsNull(CaptureOffFunction->GetTypedSemanticFunction(),
			TEXT("Capture-off compilation must not attach a semantic sidecar")));

		asUINT CaptureOffBytecodeLength = 0;
		asUINT CaptureOnBytecodeLength = 0;
		const asDWORD* const CaptureOffBytecode =
			CaptureOffFunction->GetByteCode(&CaptureOffBytecodeLength);
		const asDWORD* const CaptureOnBytecode =
			CaptureOnFunction->GetByteCode(&CaptureOnBytecodeLength);
		ASSERT_THAT(IsNotNull(CaptureOffBytecode));
		ASSERT_THAT(IsNotNull(CaptureOnBytecode));
		ASSERT_THAT(AreEqual(CaptureOffBytecodeLength, CaptureOnBytecodeLength,
			TEXT("Capture must not alter generated-function bytecode length")));
		ASSERT_THAT(IsTrue(CaptureOffBytecodeLength > 0));
		if (CaptureOffBytecode != nullptr && CaptureOnBytecode != nullptr)
		{
			ASSERT_THAT(IsTrue(FMemory::Memcmp(
				CaptureOffBytecode,
				CaptureOnBytecode,
				CaptureOffBytecodeLength * sizeof(asDWORD)) == 0,
				TEXT("Capture must not alter generated-function bytecode")));
		}

		int32 CaptureOffResult = 0;
		int32 CaptureOnResult = 0;
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOffEngine.Get(), CaptureOffFunction, CaptureOffResult)));
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOnEngine.Get(), CaptureOnFunction, CaptureOnResult)));
		ASSERT_THAT(AreEqual(42, CaptureOffResult));
		ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult,
			TEXT("Unsupported HIR metadata must not change VM execution")));

		const asCTypedSemanticFunction* const HIR =
			CaptureOnFunction->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(HIR,
			TEXT("A generated function should publish one complete inspectable HIR")));
		if (HIR == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue((HIR->header.declaredTraitBits
			& asTRAIT_GENERATED_FUNCTION) != 0));
		asUINT SynthesizedMarkerCount = 0;
		for (asUINT ExpressionIndex = 0;
			ExpressionIndex < HIR->expressions.GetLength();
			++ExpressionIndex)
		{
			const asSTypedSemanticExpression& Expression =
				HIR->expressions[ExpressionIndex];
			if (Expression.kind != asETypedSemanticExpressionKind::Unsupported
				|| Expression.unsupportedKind
					!= asETypedSemanticUnsupportedKind::CompilerSynthesizedFunction)
			{
				continue;
			}

			++SynthesizedMarkerCount;
			ASSERT_THAT(AreEqual(asUINT{0}, Expression.operands.GetLength()));
			ASSERT_THAT(IsFalse(Expression.symbol.IsValid()));
			ASSERT_THAT(AreEqual(ttVoid, Expression.type.GetTokenType()));
			ASSERT_THAT(AreEqual(
				FString(TEXT("__generated")),
				FString(UTF8_TO_TCHAR(Expression.resolvedTarget.AddressOf()))));
			ASSERT_THAT(IsTrue(Expression.span.sourceLength > 0));
		}

		ASSERT_THAT(AreEqual(asUINT{1}, SynthesizedMarkerCount,
			TEXT("The generated trait should produce one stable fallback marker")));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid(),
			TEXT("The complete generated-function fallback graph should verify")));
	}

};

#endif // WITH_ANGELSCRIPT_UNITTESTS
