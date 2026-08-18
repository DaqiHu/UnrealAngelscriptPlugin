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

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRUnsupportedContainerTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FNativeContainerStub
	{
		int32 Value = 0;
	};

public:
	TEST_METHOD(ContainerParameterRetainsIndependentTypedFallbackMarker)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-UNSUPPORTED-CONTAINER",
			ENativeEvidence::Compile
				| ENativeEvidence::Bytecode
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			int ReadTypedContainer(const TArray&in Values)
			{
				return 19;
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

		asIScriptEngine* const CaptureOffScriptEngine =
			CaptureOffEngine.Get();
		asCScriptEngine* const CaptureOnScriptEngine =
			static_cast<asCScriptEngine*>(CaptureOnEngine.Get());
		ASSERT_THAT(IsNotNull(CaptureOffScriptEngine));
		ASSERT_THAT(IsNotNull(CaptureOnScriptEngine));
		if (CaptureOffScriptEngine == nullptr || CaptureOnScriptEngine == nullptr)
		{
			return;
		}

		const asDWORD ContainerFlags =
			asOBJ_VALUE | asGetTypeTraits<FNativeContainerStub>();
		ASSERT_THAT(IsTrue(CaptureOffScriptEngine->RegisterObjectType(
			"TArray", sizeof(FNativeContainerStub), ContainerFlags) >= 0));
		ASSERT_THAT(IsTrue(CaptureOnScriptEngine->RegisterObjectType(
			"TArray", sizeof(FNativeContainerStub), ContainerFlags) >= 0));
		ASSERT_THAT(IsTrue(
			CaptureOnScriptEngine->SetTypedSemanticIRCapture(true),
			TEXT("The case-owned capture Engine should enable HIR before build")));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRUnsupportedContainerOff",
			ScriptSource);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRUnsupportedContainerOn",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid(),
			TEXT("A container parameter must remain valid with capture off")));
		ASSERT_THAT(IsTrue(CaptureOnModule.IsValid(),
			TEXT("A container parameter must remain valid with capture on")));
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		asCScriptFunction* const CaptureOffFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOffModule,
				"int ReadTypedContainer(const TArray&in Values)"));
		asCScriptFunction* const CaptureOnFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByDecl(
				CaptureOnModule,
				"int ReadTypedContainer(const TArray&in Values)"));
		ASSERT_THAT(IsNotNull(CaptureOffFunction));
		ASSERT_THAT(IsNotNull(CaptureOnFunction));
		if (CaptureOffFunction == nullptr || CaptureOnFunction == nullptr)
		{
			return;
		}

		asUINT CaptureOffBytecodeLength = 0;
		asUINT CaptureOnBytecodeLength = 0;
		ASSERT_THAT(IsNotNull(CaptureOffFunction->GetByteCode(
			&CaptureOffBytecodeLength)));
		ASSERT_THAT(IsNotNull(CaptureOnFunction->GetByteCode(
			&CaptureOnBytecodeLength)));
		ASSERT_THAT(IsTrue(CaptureOffBytecodeLength > 0));
		ASSERT_THAT(AreEqual(CaptureOffBytecodeLength,
			CaptureOnBytecodeLength,
			TEXT("HIR capture must not change container-function bytecode length")));
		ASSERT_THAT(IsNull(CaptureOffFunction->GetTypedSemanticFunction()));

		const asCTypedSemanticFunction* const HIR =
			CaptureOnFunction->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(HIR,
			TEXT("Valid container source should publish inspectable fallback HIR")));
		if (HIR == nullptr)
		{
			return;
		}

		const asSTypedSemanticExpression* ContainerMarker = nullptr;
		for (asUINT ExpressionIndex = 0;
			ExpressionIndex < HIR->expressions.GetLength();
			++ExpressionIndex)
		{
			const asSTypedSemanticExpression& Expression =
				HIR->expressions[ExpressionIndex];
			if (Expression.kind == asETypedSemanticExpressionKind::Unsupported
				&& Expression.unsupportedKind
					== asETypedSemanticUnsupportedKind::Container)
			{
				ASSERT_THAT(IsNull(ContainerMarker,
					TEXT("One container parameter should produce one container marker")));
				ContainerMarker = &Expression;
			}
		}
		ASSERT_THAT(IsNotNull(ContainerMarker,
			TEXT("Container type must not be represented only as a generic reference")));
		if (ContainerMarker == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ContainerMarker->symbol.IsValid()));
		ASSERT_THAT(IsTrue(ContainerMarker->symbol.value < HIR->symbols.GetLength()));
		if (ContainerMarker->symbol.value >= HIR->symbols.GetLength())
		{
			return;
		}
		const asSTypedSemanticSymbol& ContainerSymbol =
			HIR->symbols[ContainerMarker->symbol.value];
		ASSERT_THAT(AreEqual(asETypedSemanticSymbolKind::Parameter,
			ContainerSymbol.kind));
		ASSERT_THAT(AreEqual(FString(TEXT("Values")),
			FString(UTF8_TO_TCHAR(ContainerSymbol.name.AddressOf()))));
		ASSERT_THAT(IsTrue(ContainerMarker->type == ContainerSymbol.type,
			TEXT("The marker must preserve the exact compiler-selected container type")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("TypedSemanticIRUnsupportedContainerOn")),
			FString(UTF8_TO_TCHAR(ContainerMarker->span.section.AddressOf()))));
		ASSERT_THAT(IsTrue(ContainerMarker->span.sourceLength > 0));

		const FString Dump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(*HIR).AddressOf());
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("unsupported=Reference")),
			TEXT("The independent reference contract must remain visible")));
		ASSERT_THAT(IsTrue(Dump.Contains(TEXT("unsupported=Container")),
			TEXT("The independent container contract must be visible")));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
