#include "../../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../AngelscriptNativeTypedSemanticIREvaluationOrderTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRChainOrderTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.EvaluationOrder.Chains",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(CallAndMemberIndexChainsRetainNestedReverseOrder)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIREvaluationOrderTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-CHAIN-EVALUATION-ORDER",
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
			"TypedSemanticIRChainEvaluationOrder",
			ASTEST_AS_ANSI(R"AS(
				int Mark(int Value) { return Value; }

				struct FChain
				{
					int Value = 0;
					FChain() {}
					FChain(int InValue) { Value = InValue; }
					FChain Step(int InValue) const { return FChain(Value + InValue); }
					FChain opIndex(int InValue) const { return FChain(Value + InValue); }
					int Read() const { return Value; }
				}

				int CallChain2() { return FChain(Mark(1)).Step(Mark(2)).Read(); }
				int CallChain3() { return FChain(Mark(1)).Step(Mark(2)).Step(Mark(3)).Read(); }
				int CallChain8()
				{
					return FChain(Mark(1))
						.Step(Mark(2)).Step(Mark(3)).Step(Mark(4))
						.Step(Mark(5)).Step(Mark(6)).Step(Mark(7))
						.Step(Mark(8)).Read();
				}

				int MemberIndex2() { return FChain(Mark(1)).Step(Mark(2))[0].Value; }
				int MemberIndex3() { return FChain(Mark(1)).Step(Mark(2))[0].Step(Mark(3))[0].Value; }
				int MemberIndex8()
				{
					return FChain(Mark(1))
						.Step(Mark(2))[0].Step(Mark(3))[0].Step(Mark(4))[0]
						.Step(Mark(5))[0].Step(Mark(6))[0].Step(Mark(7))[0]
						.Step(Mark(8))[0].Value;
				}
			)AS"));
		if (!Module.IsValid())
		{
			return;
		}

		asCScriptFunction* const Marker = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(Module, "int Mark(int Value)"));
		ASSERT_THAT(IsNotNull(Marker));
		if (Marker == nullptr)
		{
			return;
		}

		struct FCase
		{
			const char* Declaration;
			int32 Count;
		};
		const FCase Cases[] = {
			{"int CallChain2()", 2},
			{"int CallChain3()", 3},
			{"int CallChain8()", 8},
			{"int MemberIndex2()", 2},
			{"int MemberIndex3()", 3},
			{"int MemberIndex8()", 8},
		};

		for (const FCase& Case : Cases)
		{
			asCScriptFunction* const Function = static_cast<asCScriptFunction*>(
				GetNativeFunctionByDecl(Module, Case.Declaration));
			const asCTypedSemanticFunction* const HIR = Function != nullptr
				? Function->GetTypedSemanticFunction()
				: nullptr;
			ASSERT_THAT(IsNotNull(HIR));
			if (HIR == nullptr)
			{
				continue;
			}

			asUINT ReceiverCallCount = 0;
			for (asUINT ExpressionIndex = 0;
				ExpressionIndex < HIR->expressions.GetLength();
				++ExpressionIndex)
			{
				const asSTypedSemanticExpression& Expression =
					HIR->expressions[ExpressionIndex];
				if (Expression.kind != asETypedSemanticExpressionKind::ResolvedCall
					|| !Expression.receiverExpression.IsValid())
				{
					continue;
				}
				++ReceiverCallCount;
				ASSERT_THAT(IsTrue(Expression.evaluationSteps.GetLength() != 0));
				if (Expression.evaluationSteps.GetLength() != 0)
				{
					const asSTypedSemanticEvaluationStep& ReceiverStep =
						Expression.evaluationSteps[
							Expression.evaluationSteps.GetLength() - 1];
					ASSERT_THAT(IsTrue(
						ReceiverStep.role
							== asETypedSemanticEvaluationRole::Receiver));
					ASSERT_THAT(IsTrue(
						ReceiverStep.expression
							== Expression.receiverExpression));
				}
			}
			ASSERT_THAT(IsTrue(ReceiverCallCount != 0,
				TEXT("Every chain needs at least one explicitly sequenced receiver call")));

			const asSTypedSemanticStatement* const Return = FindStatement(
				*HIR,
				asETypedSemanticStatementKind::Return);
			TArray<int32> Stages;
			ASSERT_THAT(IsTrue(Return != nullptr
				&& CollectMarkerStages(
					*HIR,
					Return->valueExpression,
					Marker->id,
					Stages)));
			ASSERT_THAT(IsTrue(HasExactStages(
				Stages,
				MakeExpectedStages(Case.Count, true)),
				TEXT("Nested receivers must preserve the eager suite's reverse chain order")));
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
