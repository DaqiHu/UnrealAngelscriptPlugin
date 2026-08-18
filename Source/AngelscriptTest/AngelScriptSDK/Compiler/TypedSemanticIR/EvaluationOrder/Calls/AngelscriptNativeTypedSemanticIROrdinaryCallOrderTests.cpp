#include "../../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../AngelscriptNativeTypedSemanticIREvaluationOrderTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIROrdinaryCallOrderTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.EvaluationOrder.Calls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TwoThreeAndEightArgumentsRetainReverseFormalEvaluation)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIREvaluationOrderTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-CALL-EVALUATION-ORDER",
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
			"TypedSemanticIRCallEvaluationOrder",
			ASTEST_AS_ANSI(R"AS(
				int Mark(int Value) { return Value; }
				int Collect2(int A, int B) { return A + B; }
				int Collect3(int A, int B, int C) { return A + B + C; }
				int Collect8(int A, int B, int C, int D, int E, int F, int G, int H)
				{
					return A + B + C + D + E + F + G + H;
				}

				int Run2() { return Collect2(Mark(1), Mark(2)); }
				int Run3() { return Collect3(Mark(1), Mark(2), Mark(3)); }
				int Run8()
				{
					return Collect8(
						Mark(1), Mark(2), Mark(3), Mark(4),
						Mark(5), Mark(6), Mark(7), Mark(8));
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
			const char* RunDeclaration;
			const char* TargetDeclaration;
			int32 Count;
		};
		const FCase Cases[] = {
			{"int Run2()", "int Collect2(int A, int B)", 2},
			{"int Run3()", "int Collect3(int A, int B, int C)", 3},
			{"int Run8()", "int Collect8(int A, int B, int C, int D, int E, int F, int G, int H)", 8},
		};

		for (const FCase& Case : Cases)
		{
			asCScriptFunction* const Run = static_cast<asCScriptFunction*>(
				GetNativeFunctionByDecl(Module, Case.RunDeclaration));
			asCScriptFunction* const Target = static_cast<asCScriptFunction*>(
				GetNativeFunctionByDecl(Module, Case.TargetDeclaration));
			const asCTypedSemanticFunction* const HIR = Run != nullptr
				? Run->GetTypedSemanticFunction()
				: nullptr;
			ASSERT_THAT(IsNotNull(HIR));
			ASSERT_THAT(IsNotNull(Target));
			if (HIR == nullptr || Target == nullptr)
			{
				continue;
			}

			const asSTypedSemanticExpression* const Call =
				FindResolvedCall(*HIR, Target->id);
			ASSERT_THAT(IsNotNull(Call));
			if (Call == nullptr)
			{
				continue;
			}

			ASSERT_THAT(AreEqual(Case.Count,
				static_cast<int32>(Call->operands.GetLength())));
			ASSERT_THAT(AreEqual(Case.Count,
				static_cast<int32>(Call->sourceArgumentOrdinals.GetLength())));
			ASSERT_THAT(AreEqual(Case.Count,
				static_cast<int32>(Call->evaluationSteps.GetLength())));
			for (int32 Formal = 0; Formal < Case.Count; ++Formal)
			{
				const int32 Binding = FindFormalBinding(*Call, Formal);
				ASSERT_THAT(AreNotEqual(INDEX_NONE, Binding));
				if (Binding != INDEX_NONE)
				{
					ASSERT_THAT(AreEqual(
						Formal,
						Call->sourceArgumentOrdinals[Binding]));
				}
			}
			for (int32 EvaluationOrdinal = 0;
				EvaluationOrdinal < Case.Count;
				++EvaluationOrdinal)
			{
				const int32 ExpectedFormal = Case.Count - EvaluationOrdinal - 1;
				const int32 Binding = FindFormalBinding(*Call, ExpectedFormal);
				const asSTypedSemanticEvaluationStep& Step =
					Call->evaluationSteps[EvaluationOrdinal];
				ASSERT_THAT(IsTrue(
					Step.role == asETypedSemanticEvaluationRole::Argument));
				ASSERT_THAT(AreEqual(
					ExpectedFormal,
					Step.formalParameterIndex));
				ASSERT_THAT(AreEqual(
					ExpectedFormal,
					Step.sourceArgumentOrdinal));
				ASSERT_THAT(IsTrue(Binding != INDEX_NONE
					&& Step.expression == Call->operands[Binding]));
			}

			const asSTypedSemanticStatement* const Return = FindStatement(
				*HIR,
				asETypedSemanticStatementKind::Return);
			ASSERT_THAT(IsNotNull(Return));
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
				TEXT("The call HIR must match the maintained reverse-formal VM oracle")));
			ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*HIR).IsValid()));
		}

		asCScriptFunction* const Run3 = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(Module, "int Run3()"));
		asCScriptFunction* const Collect3 = static_cast<asCScriptFunction*>(
			GetNativeFunctionByDecl(
				Module,
				"int Collect3(int A, int B, int C)"));
		if (Run3 != nullptr && Collect3 != nullptr
			&& Run3->GetTypedSemanticFunction() != nullptr)
		{
			asCTypedSemanticFunction Invalid =
				*Run3->GetTypedSemanticFunction();
			asSTypedSemanticExpression* InvalidCall = nullptr;
			for (asUINT Index = 0; Index < Invalid.expressions.GetLength(); ++Index)
			{
				if (Invalid.expressions[Index].kind
						== asETypedSemanticExpressionKind::ResolvedCall
					&& Invalid.expressions[Index].resolvedFunctionId
						== Collect3->id)
				{
					InvalidCall = &Invalid.expressions[Index];
					break;
				}
			}
			ASSERT_THAT(IsNotNull(InvalidCall));
			if (InvalidCall != nullptr
				&& InvalidCall->evaluationSteps.GetLength() >= 2)
			{
				InvalidCall->evaluationSteps[0].formalParameterIndex =
					InvalidCall->evaluationSteps[1].formalParameterIndex;
				ASSERT_THAT(AreEqual(
					asETypedSemanticVerificationError::InvalidCallEvaluationSequence,
					VerifyTypedSemanticFunction(Invalid).error));
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
