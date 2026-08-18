#include "../../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../AngelscriptNativeTypedSemanticIREvaluationOrderTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRExpressionOrderTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.EvaluationOrder.Expressions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(BinaryAssignmentCompoundAndNestedCastsUseTheirOwnOrder)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIREvaluationOrderTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-EXPRESSION-EVALUATION-ORDER",
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
			"TypedSemanticIRExpressionEvaluationOrder",
			ASTEST_AS_ANSI(R"AS(
				int Mark(int Value) { return Value; }

				int Binary2() { return Mark(1) + Mark(2); }
				int Binary3() { return Mark(1) + Mark(2) + Mark(3); }
				int Binary8()
				{
					return Mark(1) + Mark(2) + Mark(3) + Mark(4)
						+ Mark(5) + Mark(6) + Mark(7) + Mark(8);
				}

				int Assign2() { int Target = 0; Target = Mark(1) + Mark(2); return Target; }
				int Assign3() { int Target = 0; Target = Mark(1) + Mark(2) + Mark(3); return Target; }
				int Assign8()
				{
					int Target = 0;
					Target = Mark(1) + Mark(2) + Mark(3) + Mark(4)
						+ Mark(5) + Mark(6) + Mark(7) + Mark(8);
					return Target;
				}

				int Compound2() { int Target = 10; Target += Mark(1) + Mark(2); return Target; }
				int Compound3() { int Target = 10; Target += Mark(1) + Mark(2) + Mark(3); return Target; }
				int Compound8()
				{
					int Target = 10;
					Target += Mark(1) + Mark(2) + Mark(3) + Mark(4)
						+ Mark(5) + Mark(6) + Mark(7) + Mark(8);
					return Target;
				}

				int Cast2() { return int(float(double(Mark(1) + Mark(2)))); }
				int Cast3() { return int(int(float(double(Mark(1) + Mark(2) + Mark(3))))); }
				int Cast8()
				{
					return int(float(int(double(float(int(double(float(
						Mark(1) + Mark(2) + Mark(3) + Mark(4)
						+ Mark(5) + Mark(6) + Mark(7) + Mark(8)))))))));
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

		enum class ECaseKind : uint8
		{
			Binary,
			Assignment,
			Compound,
			Cast,
		};
		struct FCase
		{
			const char* Declaration;
			int32 Count;
			ECaseKind Kind;
		};
		const FCase Cases[] = {
			{"int Binary2()", 2, ECaseKind::Binary},
			{"int Binary3()", 3, ECaseKind::Binary},
			{"int Binary8()", 8, ECaseKind::Binary},
			{"int Assign2()", 2, ECaseKind::Assignment},
			{"int Assign3()", 3, ECaseKind::Assignment},
			{"int Assign8()", 8, ECaseKind::Assignment},
			{"int Compound2()", 2, ECaseKind::Compound},
			{"int Compound3()", 3, ECaseKind::Compound},
			{"int Compound8()", 8, ECaseKind::Compound},
			{"int Cast2()", 2, ECaseKind::Cast},
			{"int Cast3()", 3, ECaseKind::Cast},
			{"int Cast8()", 8, ECaseKind::Cast},
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

			const asSTypedSemanticStatement* RootStatement = nullptr;
			if (Case.Kind == ECaseKind::Assignment
				|| Case.Kind == ECaseKind::Compound)
			{
				RootStatement = FindStatement(
					*HIR,
					asETypedSemanticStatementKind::Expression,
					asETypedSemanticExpressionKind::Assignment);
			}
			else
			{
				RootStatement = FindStatement(
					*HIR,
					asETypedSemanticStatementKind::Return);
			}
			ASSERT_THAT(IsNotNull(RootStatement));
			if (RootStatement == nullptr)
			{
				continue;
			}

			const asSTypedSemanticExpression* const Root = ResolveExpression(
				*HIR,
				RootStatement->valueExpression);
			ASSERT_THAT(IsNotNull(Root));
			if (Root == nullptr)
			{
				continue;
			}

			if (Case.Kind == ECaseKind::Assignment
				|| Case.Kind == ECaseKind::Compound)
			{
				ASSERT_THAT(AreEqual(2,
					static_cast<int32>(Root->evaluationSteps.GetLength())));
				if (Root->evaluationSteps.GetLength() == 2)
				{
					ASSERT_THAT(IsTrue(
						Root->evaluationSteps[0].role
							== asETypedSemanticEvaluationRole::MutationValue));
					ASSERT_THAT(IsTrue(
						Root->evaluationSteps[0].expression
							== Root->mutationValue));
					ASSERT_THAT(IsTrue(
						Root->evaluationSteps[1].role
							== asETypedSemanticEvaluationRole::MutationTarget));
					ASSERT_THAT(IsTrue(
						Root->evaluationSteps[1].expression
							== Root->mutationTarget));
				}
			}

			for (asUINT ExpressionIndex = 0;
				ExpressionIndex < HIR->expressions.GetLength();
				++ExpressionIndex)
			{
				const asSTypedSemanticExpression& Expression =
					HIR->expressions[ExpressionIndex];
				if (Expression.kind == asETypedSemanticExpressionKind::Binary)
				{
					ASSERT_THAT(AreEqual(2,
						static_cast<int32>(Expression.evaluationSteps.GetLength())));
					if (Expression.evaluationSteps.GetLength() == 2)
					{
						ASSERT_THAT(IsTrue(
							Expression.evaluationSteps[0].expression
								== Expression.operands[0]));
						ASSERT_THAT(IsTrue(
							Expression.evaluationSteps[1].expression
								== Expression.operands[1]));
					}
				}
				else if (Expression.kind
					== asETypedSemanticExpressionKind::Conversion)
				{
					ASSERT_THAT(AreEqual(1,
						static_cast<int32>(Expression.evaluationSteps.GetLength())));
					if (Expression.evaluationSteps.GetLength() == 1)
					{
						ASSERT_THAT(IsTrue(
							Expression.evaluationSteps[0].expression
								== Expression.operands[0]));
					}
				}
			}

			TArray<int32> Stages;
			ASSERT_THAT(IsTrue(CollectMarkerStages(
				*HIR,
				RootStatement->valueExpression,
				Marker->id,
				Stages)));
			ASSERT_THAT(IsTrue(HasExactStages(
				Stages,
				MakeExpectedStages(Case.Count, false)),
				TEXT("Non-call eager expressions must retain the maintained left-to-right oracle")));
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
