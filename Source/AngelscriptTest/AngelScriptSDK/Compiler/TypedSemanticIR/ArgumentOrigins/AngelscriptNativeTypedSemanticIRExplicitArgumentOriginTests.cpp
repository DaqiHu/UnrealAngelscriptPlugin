#include "../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptNativeTypedSemanticIRArgumentOriginTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRExplicitArgumentOriginTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.ArgumentOrigins.Explicit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(PositionalAndNamedSourceRolesRemainSeparateFromFormalAndEvaluationOrder)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIRArgumentOriginTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-EXPLICIT-ARGUMENT-ORIGIN",
			ENativeEvidence::Compile
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

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

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"TypedSemanticIRExplicitArgumentOrigin",
			ASTEST_AS_ANSI(R"AS(
				int Mark(int Value) { return Value; }
				int Collect(int Alpha, int Beta, int Gamma)
				{
					return Alpha * 100 + Beta * 10 + Gamma;
				}

				int Positional()
				{
					return Collect(Mark(1), Mark(2), Mark(3));
				}

				int Named()
				{
					return Collect(
						Gamma: Mark(3), Alpha: Mark(1), Beta: Mark(2));
				}
			)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		asCScriptFunction* const Target = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(
				Module,
				"int Collect(int Alpha, int Beta, int Gamma)"));
		asCScriptFunction* const Positional = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(Module, "int Positional()"));
		asCScriptFunction* const Named = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(Module, "int Named()"));
		ASSERT_THAT(IsNotNull(Target));
		ASSERT_THAT(IsNotNull(Positional));
		ASSERT_THAT(IsNotNull(Named));
		if (Target == nullptr || Positional == nullptr || Named == nullptr)
		{
			return;
		}

		const asSTypedSemanticExpression* const PositionalCall =
			Positional->GetTypedSemanticFunction() != nullptr
				? FindResolvedCall(
					*Positional->GetTypedSemanticFunction(),
					Target->id)
				: nullptr;
		const asSTypedSemanticExpression* const NamedCall =
			Named->GetTypedSemanticFunction() != nullptr
				? FindResolvedCall(*Named->GetTypedSemanticFunction(), Target->id)
				: nullptr;
		ASSERT_THAT(IsNotNull(PositionalCall));
		ASSERT_THAT(IsNotNull(NamedCall));
		if (PositionalCall == nullptr || NamedCall == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(3,
			static_cast<int32>(PositionalCall->argumentProvenance.GetLength())));
		ASSERT_THAT(AreEqual(3,
			static_cast<int32>(NamedCall->argumentProvenance.GetLength())));
		const int32 ExpectedNamedSourceOrdinals[] = {1, 2, 0};
		const ANSICHAR* const ExpectedNames[] = {"Alpha", "Beta", "Gamma"};
		for (int32 Formal = 0; Formal < 3; ++Formal)
		{
			const int32 PositionalBinding = FindFormalBinding(
				*PositionalCall,
				Formal);
			const int32 NamedBinding = FindFormalBinding(*NamedCall, Formal);
			ASSERT_THAT(AreNotEqual(INDEX_NONE, PositionalBinding));
			ASSERT_THAT(AreNotEqual(INDEX_NONE, NamedBinding));
			if (PositionalBinding == INDEX_NONE || NamedBinding == INDEX_NONE)
			{
				continue;
			}

			const asSTypedSemanticArgumentProvenance& PositionalOrigin =
				PositionalCall->argumentProvenance[PositionalBinding];
			ASSERT_THAT(IsTrue(PositionalOrigin.origin
				== asETypedSemanticArgumentOrigin::SourcePositional));
			ASSERT_THAT(AreEqual(Formal, PositionalOrigin.sourceArgumentOrdinal));
			ASSERT_THAT(IsTrue(PositionalOrigin.sourceArgumentName == ""));
			ASSERT_THAT(IsTrue(PositionalOrigin.processedSpan.sourceLength != 0));

			const asSTypedSemanticArgumentProvenance& NamedOrigin =
				NamedCall->argumentProvenance[NamedBinding];
			ASSERT_THAT(IsTrue(NamedOrigin.origin
				== asETypedSemanticArgumentOrigin::SourceNamed));
			ASSERT_THAT(AreEqual(
				ExpectedNamedSourceOrdinals[Formal],
				NamedOrigin.sourceArgumentOrdinal));
			ASSERT_THAT(IsTrue(
				NamedOrigin.sourceArgumentName == ExpectedNames[Formal]));
			ASSERT_THAT(IsTrue(NamedOrigin.processedSpan.sourceLength != 0));
		}

		ASSERT_THAT(IsTrue(
			VerifyTypedSemanticFunction(
				*Positional->GetTypedSemanticFunction()).IsValid()));
		ASSERT_THAT(IsTrue(
			VerifyTypedSemanticFunction(
				*Named->GetTypedSemanticFunction()).IsValid()));
		const FString NamedDump = UTF8_TO_TCHAR(
			DumpTypedSemanticFunction(
				*Named->GetTypedSemanticFunction()).AddressOf());
		ASSERT_THAT(IsTrue(NamedDump.Contains(TEXT("SourceNamed"))));
		ASSERT_THAT(IsTrue(NamedDump.Contains(TEXT("name=Alpha"))));

		asCTypedSemanticFunction Invalid = *Named->GetTypedSemanticFunction();
		for (asUINT ExpressionIndex = 0;
			ExpressionIndex < Invalid.expressions.GetLength();
			++ExpressionIndex)
		{
			asSTypedSemanticExpression& Expression =
				Invalid.expressions[ExpressionIndex];
			if (Expression.kind
					== asETypedSemanticExpressionKind::ResolvedCall
				&& Expression.resolvedFunctionId == Target->id)
			{
				Expression.argumentProvenance[0].sourceArgumentName.Clear();
				break;
			}
		}
		ASSERT_THAT(AreEqual(
			asETypedSemanticVerificationError::InvalidArgumentProvenance,
			VerifyTypedSemanticFunction(Invalid).error));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
