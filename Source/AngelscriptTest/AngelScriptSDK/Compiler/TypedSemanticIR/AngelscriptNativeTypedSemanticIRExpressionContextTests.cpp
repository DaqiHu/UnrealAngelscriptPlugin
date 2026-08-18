#include "../../Support/AngelscriptNativeBuilderTestSupport.h"
#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_compiler.h"
#include "source/as_module.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRExpressionContextTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ExpressionContextCopyMergeAndClearPropagateTypedIdentity)
	{
		using namespace AngelscriptBuilderTestSupport;
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-EXPRESSION-CONTEXT",
			ENativeEvidence::Metadata | ENativeEvidence::Lifecycle | ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		FScopedNativeModuleName ModuleScope(
			Engine,
			"TypedSemanticIRExpressionContext");
		asCModule* const Module = CreateBuilderModule(
			Engine.Get(),
			ModuleScope.Get());
		ASSERT_THAT(IsNotNull(Module,
			TEXT("Expression-context coverage should create a case-owned module")));
		if (Module == nullptr)
		{
			return;
		}
		ASSERT_THAT(IsTrue(AddBuilderSection(
			*Module,
			"TypedSemanticIRExpressionContext.as",
			"int Entry() { return 1; }"),
			TEXT("Expression-context coverage should create a live builder")));
		asCBuilder* const Builder = Module->builder;
		ASSERT_THAT(IsNotNull(Builder,
			TEXT("Expression-context coverage should receive the module builder")));
		if (Builder == nullptr)
		{
			return;
		}

		asCExprContext Source(Builder);
		ASSERT_THAT(IsFalse(Source.typedSemanticExpression.IsValid(),
			TEXT("A newly constructed expression context should have no typed identity")));
		Source.typedSemanticExpression = asTypedSemanticExpressionId(7);

		asCExprContext Copied(Builder);
		Copied.Copy(&Source);
		ASSERT_THAT(AreEqual(
			7,
			static_cast<int32>(Copied.typedSemanticExpression.value),
			TEXT("Copy should preserve the exact typed expression identity")));

		asCExprContext Merged(Builder);
		Merged.Merge(&Source);
		ASSERT_THAT(AreEqual(
			7,
			static_cast<int32>(Merged.typedSemanticExpression.value),
			TEXT("Merge should preserve the after-context typed expression identity")));

		Copied.Clear();
		ASSERT_THAT(IsFalse(Copied.typedSemanticExpression.IsValid(),
			TEXT("Clear should invalidate the typed expression identity")));
		Merged.SetVoidExpression();
		ASSERT_THAT(IsFalse(Merged.typedSemanticExpression.IsValid(),
			TEXT("An explicit void expression should not retain a typed expression identity")));
	}
};

#endif
