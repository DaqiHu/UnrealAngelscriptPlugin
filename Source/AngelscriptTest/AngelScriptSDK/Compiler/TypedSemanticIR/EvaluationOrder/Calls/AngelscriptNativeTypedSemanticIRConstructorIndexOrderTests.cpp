#include "../../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../AngelscriptNativeTypedSemanticIREvaluationOrderTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRConstructorIndexOrderTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.EvaluationOrder.ConstructorIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TwoThreeAndEightOperandsRetainResolvedCallsAndReceiverPosition)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIREvaluationOrderTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-CONSTRUCTOR-INDEX-ORDER",
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
			"TypedSemanticIRConstructorIndexOrder",
			ASTEST_AS_ANSI(R"AS(
				int Mark(int Value) { return Value; }

				struct FConstructed
				{
					int Value = 0;
					FConstructed(int A, int B) { Value = A + B; }
					FConstructed(int A, int B, int C) { Value = A + B + C; }
					FConstructed(int A, int B, int C, int D, int E, int F, int G, int H)
					{
						Value = A + B + C + D + E + F + G + H;
					}
				}

				struct FIndexer
				{
					int opIndex(int A, int B) const { return A + B; }
					int opIndex(int A, int B, int C) const { return A + B + C; }
					int opIndex(int A, int B, int C, int D, int E, int F, int G, int H) const
					{
						return A + B + C + D + E + F + G + H;
					}
				}

				int Construct2() { return FConstructed(Mark(1), Mark(2)).Value; }
				int Construct3() { return FConstructed(Mark(1), Mark(2), Mark(3)).Value; }
				int Construct8()
				{
					return FConstructed(
						Mark(1), Mark(2), Mark(3), Mark(4),
						Mark(5), Mark(6), Mark(7), Mark(8)).Value;
				}

				int Index2() { FIndexer Value; return Value[Mark(1), Mark(2)]; }
				int Index3() { FIndexer Value; return Value[Mark(1), Mark(2), Mark(3)]; }
				int Index8()
				{
					FIndexer Value;
					return Value[
						Mark(1), Mark(2), Mark(3), Mark(4),
						Mark(5), Mark(6), Mark(7), Mark(8)];
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
			bool bIndex;
		};
		const FCase Cases[] = {
			{"int Construct2()", 2, false},
			{"int Construct3()", 3, false},
			{"int Construct8()", 8, false},
			{"int Index2()", 2, true},
			{"int Index3()", 3, true},
			{"int Index8()", 8, true},
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

			const asSTypedSemanticExpression* const Call = Case.bIndex
				? FindResolvedCallByName(*HIR, *ScriptEngine, "opIndex")
				: FindConstructorCall(*HIR, *ScriptEngine);
			ASSERT_THAT(IsNotNull(Call,
				TEXT("Constructor and index syntax must retain the selected callable HIR")));
			if (Call == nullptr)
			{
				continue;
			}

			const int32 ExpectedStepCount = Case.Count + (Case.bIndex ? 1 : 0);
			ASSERT_THAT(AreEqual(
				ExpectedStepCount,
				static_cast<int32>(Call->evaluationSteps.GetLength())));
			for (int32 StepIndex = 0; StepIndex < Case.Count; ++StepIndex)
			{
				const asSTypedSemanticEvaluationStep& Step =
					Call->evaluationSteps[StepIndex];
				ASSERT_THAT(IsTrue(
					Step.role == asETypedSemanticEvaluationRole::Argument));
				ASSERT_THAT(AreEqual(
					Case.Count - StepIndex - 1,
					Step.formalParameterIndex));
			}
			if (Case.bIndex)
			{
				ASSERT_THAT(IsTrue(Call->receiverExpression.IsValid()));
				const asSTypedSemanticEvaluationStep& ReceiverStep =
					Call->evaluationSteps[
						Call->evaluationSteps.GetLength() - 1];
				ASSERT_THAT(IsTrue(
					ReceiverStep.role == asETypedSemanticEvaluationRole::Receiver));
				ASSERT_THAT(IsTrue(
					ReceiverStep.expression == Call->receiverExpression));
			}
			else
			{
				ASSERT_THAT(IsFalse(Call->receiverExpression.IsValid()));
			}

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
				MakeExpectedStages(Case.Count, true))));
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
