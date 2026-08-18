#include "../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptNativeTypedSemanticIRArgumentOriginTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRDefaultArgumentOriginTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.ArgumentOrigins.Default",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(DefaultBindingsRetainCalleeParameterCanonicalTextCallerSpanAndSignatureDependency)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIRArgumentOriginTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-DEFAULT-ARGUMENT-ORIGIN",
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
			"TypedSemanticIRDefaultArgumentOrigin",
			ASTEST_AS_ANSI(R"AS(
				int Mark(int Value) { return Value; }
				int Collect(int Alpha, int Beta = 20, int Gamma = 30)
				{
					return Alpha * 100 + Beta * 10 + Gamma;
				}

				int Defaults()
				{
					return Collect(Mark(1));
				}

				int NamedPartial()
				{
					return Collect(Alpha: Mark(1), Gamma: Mark(3));
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
		asCScriptFunction* const Defaults = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(Module, "int Defaults()"));
		asCScriptFunction* const NamedPartial = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(Module, "int NamedPartial()"));
		ASSERT_THAT(IsNotNull(Target));
		ASSERT_THAT(IsNotNull(Defaults));
		ASSERT_THAT(IsNotNull(NamedPartial));
		if (Target == nullptr || Defaults == nullptr || NamedPartial == nullptr)
		{
			return;
		}

		const asCString ExpectedDeclaration = Target->GetDeclarationStr(
			true,
			true,
			true,
			false,
			false);
		struct FCase
		{
			asCScriptFunction* Caller;
			int32 DefaultFormalBegin;
		};
		const FCase Cases[] = {
			{Defaults, 1},
			{NamedPartial, 1},
		};

		for (const FCase& Case : Cases)
		{
			const asCTypedSemanticFunction* const HIR =
				Case.Caller->GetTypedSemanticFunction();
			const asSTypedSemanticExpression* const Call = HIR != nullptr
				? FindResolvedCall(*HIR, Target->id)
				: nullptr;
			ASSERT_THAT(IsNotNull(HIR));
			ASSERT_THAT(IsNotNull(Call));
			if (HIR == nullptr || Call == nullptr)
			{
				continue;
			}

			const int32 ExpectedDefaultEnd = Case.Caller == Defaults ? 3 : 2;
			for (int32 Formal = Case.DefaultFormalBegin;
				Formal < ExpectedDefaultEnd;
				++Formal)
			{
				const int32 Binding = FindFormalBinding(*Call, Formal);
				ASSERT_THAT(AreNotEqual(INDEX_NONE, Binding));
				if (Binding == INDEX_NONE)
				{
					continue;
				}
				const asSTypedSemanticArgumentProvenance& Origin =
					Call->argumentProvenance[Binding];
				ASSERT_THAT(IsTrue(Origin.origin
					== asETypedSemanticArgumentOrigin::Default));
				ASSERT_THAT(AreEqual(-1, Origin.sourceArgumentOrdinal));
				ASSERT_THAT(IsTrue(Origin.sourceArgumentName == ""));
				ASSERT_THAT(AreEqual(Target->id, Origin.calleeFunctionId));
				ASSERT_THAT(IsTrue(
					Origin.calleeDeclaration == ExpectedDeclaration));
				ASSERT_THAT(AreEqual(Formal, Origin.calleeParameterIndex));
				ASSERT_THAT(IsTrue(Origin.calleeParameterName
					== Target->parameterNames[Formal]));
				ASSERT_THAT(IsTrue(Origin.canonicalExpression
					== (Formal == 1 ? "20" : "30")));
				ASSERT_THAT(IsTrue(AreSpansEqual(
					Origin.processedSpan,
					Call->span),
					TEXT("A default expression must retain the caller processed call span used for diagnostics")));
			}

			ASSERT_THAT(AreEqual(
				1,
				CountSignatureDependencies(*Case.Caller, *Target),
				TEXT("The caller must keep the compiler's authoritative Signature dependency instead of inventing a HIR invalidation edge")));
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
			const FString Dump = UTF8_TO_TCHAR(
				DumpTypedSemanticFunction(*HIR).AddressOf());
			ASSERT_THAT(IsTrue(Dump.Contains(TEXT("origin=Default"))));
			ASSERT_THAT(IsTrue(Dump.Contains(TEXT("canonical=20"))));
		}

		asCTypedSemanticFunction Invalid =
			*Defaults->GetTypedSemanticFunction();
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
				const int32 DefaultBinding = FindFormalBinding(Expression, 1);
				if (DefaultBinding != INDEX_NONE)
				{
					Expression.argumentProvenance[DefaultBinding]
						.canonicalExpression.Clear();
				}
				break;
			}
		}
		ASSERT_THAT(AreEqual(
			asETypedSemanticVerificationError::InvalidArgumentProvenance,
			VerifyTypedSemanticFunction(Invalid).error));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
