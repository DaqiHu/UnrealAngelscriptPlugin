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

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRUnsupportedPropertyTests,
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
	TEST_METHOD(ObjectPropertyFallbackRetainsReceiverTargetTypeAndSpan)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-UNSUPPORTED-PROPERTY",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			class FTypedPropertyCarrier
			{
				int Value;
			}

			int ReadTypedProperty(const FTypedPropertyCarrier&in Carrier)
			{
				return Carrier.Value;
			}

			int RunTypedProperty()
			{
				FTypedPropertyCarrier Carrier = FTypedPropertyCarrier();
				Carrier.Value = 37;
				return ReadTypedProperty(Carrier);
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
			"TypedSemanticIRUnsupportedPropertyOff",
			ScriptSource);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRUnsupportedPropertyOn",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid(),
			TEXT("Unsupported property syntax must remain valid with capture off")));
		ASSERT_THAT(IsTrue(CaptureOnModule.IsValid(),
			TEXT("Unsupported property syntax must remain valid with capture on")));
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		asCScriptFunction* const CaptureOffRead =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOffModule,
				"int ReadTypedProperty(const FTypedPropertyCarrier&in Carrier)"));
		asCScriptFunction* const CaptureOnRead =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOnModule,
				"int ReadTypedProperty(const FTypedPropertyCarrier&in Carrier)"));
		asIScriptFunction* const CaptureOffRun = GetNativeFunctionByDecl(
			CaptureOffModule,
			"int RunTypedProperty()");
		asIScriptFunction* const CaptureOnRun = GetNativeFunctionByDecl(
			CaptureOnModule,
			"int RunTypedProperty()");
		ASSERT_THAT(IsNotNull(CaptureOffRead));
		ASSERT_THAT(IsNotNull(CaptureOnRead));
		ASSERT_THAT(IsNotNull(CaptureOffRun));
		ASSERT_THAT(IsNotNull(CaptureOnRun));
		if (CaptureOffRead == nullptr || CaptureOnRead == nullptr
			|| CaptureOffRun == nullptr || CaptureOnRun == nullptr)
		{
			return;
		}

		asUINT CaptureOffBytecodeLength = 0;
		asUINT CaptureOnBytecodeLength = 0;
		ASSERT_THAT(IsNotNull(CaptureOffRead->GetByteCode(
			&CaptureOffBytecodeLength)));
		ASSERT_THAT(IsNotNull(CaptureOnRead->GetByteCode(
			&CaptureOnBytecodeLength)));
		ASSERT_THAT(IsTrue(CaptureOffBytecodeLength > 0));
		ASSERT_THAT(IsTrue(CaptureOnBytecodeLength > 0));
		ASSERT_THAT(IsNull(CaptureOffRead->GetTypedSemanticFunction(),
			TEXT("Capture-off compilation must not attach a semantic sidecar")));

		int32 CaptureOffResult = 0;
		int32 CaptureOnResult = 0;
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOffEngine.Get(),
			CaptureOffRun,
			CaptureOffResult)));
		ASSERT_THAT(IsTrue(ExecuteGlobalInt(
			CaptureOnEngine.Get(),
			CaptureOnRun,
			CaptureOnResult)));
		ASSERT_THAT(AreEqual(37, CaptureOffResult));
		ASSERT_THAT(AreEqual(CaptureOffResult, CaptureOnResult,
			TEXT("HIR fallback metadata must not change VM property behavior")));

		const asCTypedSemanticFunction* const HIR =
			CaptureOnRead->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(HIR,
			TEXT("Valid unsupported property access should publish inspectable HIR")));
		if (HIR == nullptr)
		{
			return;
		}

		const asSTypedSemanticExpression* PropertyAccess = nullptr;
		for (asUINT ExpressionIndex = 0;
			ExpressionIndex < HIR->expressions.GetLength();
			++ExpressionIndex)
		{
			const asSTypedSemanticExpression& Expression =
				HIR->expressions[ExpressionIndex];
			if (Expression.kind == asETypedSemanticExpressionKind::Unsupported
				&& Expression.unsupportedKind
					== asETypedSemanticUnsupportedKind::PropertyAccess)
			{
				ASSERT_THAT(IsNull(PropertyAccess,
					TEXT("One source property access should produce one fallback node")));
				PropertyAccess = &Expression;
			}
		}
		ASSERT_THAT(IsNotNull(PropertyAccess,
			TEXT("Direct object property access must not degrade to its receiver symbol")));
		if (PropertyAccess == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(asUINT{1}, PropertyAccess->operands.GetLength(),
			TEXT("The property marker must retain its resolved receiver operand")));
		ASSERT_THAT(IsTrue(PropertyAccess->operands.GetLength() == 1
			&& PropertyAccess->operands[0].value
				< PropertyAccess->id.value,
			TEXT("The receiver must be captured before the property marker")));
		ASSERT_THAT(IsTrue(PropertyAccess->type.IsEqualExceptRefAndConst(
			asCDataType::CreatePrimitive(ttInt, false)),
			TEXT("The marker must preserve the compiler-selected property type")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("TypedSemanticIRUnsupportedPropertyOn")),
			FString(UTF8_TO_TCHAR(PropertyAccess->span.section.AddressOf())),
			TEXT("The marker must retain the processed compiler section")));
		ASSERT_THAT(IsTrue(PropertyAccess->span.sourceLength > 0));

		const FString Dump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*HIR).AddressOf());
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("unsupported=PropertyAccess"))));
		ASSERT_THAT(IsTrue(Dump.Contains(
			TEXT("target=FTypedPropertyCarrier::Value")),
			TEXT("The pointer-free dump must expose the resolved property target")));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid(),
			TEXT("The complete unsupported marker must pass structural verification")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
