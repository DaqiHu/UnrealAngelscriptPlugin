#include "../../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../../Support/AngelscriptNativeCoreTestSupport.h"
#include "AngelscriptNativeTypedSemanticIRMixinTestSupport.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FTypedSemanticIRMixinCallTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler.TypedSemanticIR.Mixin.Call",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(MethodSyntaxRetainsSingleReceiverAndFinalFormalOrder)
	{
		using namespace AngelscriptNativeTestSupport;
		using namespace TypedSemanticIRMixinTestSupport;

		AS_NATIVE_PRODUCT(
			"COMPILER-TYPED-SEMANTIC-IR-MIXIN-CALL",
			ENativeEvidence::Compile
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Isolation);

		const std::string ScriptSource = ASTEST_AS_ANSI(R"AS(
			struct FMixinValue
			{
				int Value = 0;
			}

			struct FMixinState
			{
				FMixinValue Receiver;
				int Trace = 0;
				int ReceiverCount = 0;
				int ArgumentCount = 0;

				FMixinValue& SelectReceiver()
				{
					Trace = Trace * 10 + 1;
					ReceiverCount++;
					return Receiver;
				}

				int SelectArgument(int Stage, int InValue)
				{
					Trace = Trace * 10 + Stage;
					ArgumentCount++;
					return InValue;
				}
			}

			mixin int EvaluateMixin(
				FMixinValue& Self,
				int A,
				int B = 5,
				int C = 7)
			{
				return Self.Value * 100 + A * 10 + B + C;
			}

			int ReadExternal(FMixinValue& Self) external_implicit_this
			{
				return this.Value;
			}

			int RunMixin()
			{
				FMixinState State;
				State.Receiver.Value = 2;
				int Result = State.SelectReceiver().EvaluateMixin(
					C: State.SelectArgument(3, 7),
					A: State.SelectArgument(2, 4));
				return State.Trace * 1000000
					+ State.ReceiverCount * 100000
					+ State.ArgumentCount * 10000
					+ Result;
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
			CaptureOnScriptEngine->SetTypedSemanticIRCapture(true)));

		FScopedNativeModule CaptureOffModule(
			*TestRunner,
			CaptureOffEngine,
			"TypedSemanticIRMixinCallOff",
			ScriptSource);
		FScopedNativeModule CaptureOnModule(
			*TestRunner,
			CaptureOnEngine,
			"TypedSemanticIRMixinCallOn",
			ScriptSource);
		ASSERT_THAT(IsTrue(CaptureOffModule.IsValid()));
		ASSERT_THAT(IsTrue(CaptureOnModule.IsValid()));
		if (!CaptureOffModule.IsValid() || !CaptureOnModule.IsValid())
		{
			return;
		}

		asIScriptFunction* const CaptureOffRun = GetNativeFunctionByExactDecl(
			CaptureOffModule,
			"int RunMixin()");
		asCScriptFunction* const CaptureOnRun =
			static_cast<asCScriptFunction*>(GetNativeFunctionByExactDecl(
				CaptureOnModule,
				"int RunMixin()"));
		asCScriptFunction* const MixinFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByExactDecl(
				CaptureOnModule,
				"int EvaluateMixin(FMixinValue&inout Self, const int A, const int B, const int C)"));
		asCScriptFunction* const ExternalFunction =
			static_cast<asCScriptFunction*>(GetNativeFunctionByExactDecl(
				CaptureOnModule,
				"int ReadExternal(FMixinValue&inout Self)"));
		asITypeInfo* const StateType =
			CaptureOnModule->GetTypeInfoByDecl("FMixinState");
		asCScriptFunction* const SelectReceiver =
			static_cast<asCScriptFunction*>(GetNativeMethodByDecl(
				StateType,
				"FMixinValue& SelectReceiver()"));
		asCScriptFunction* const SelectArgument =
			static_cast<asCScriptFunction*>(GetNativeMethodByDecl(
				StateType,
				"int SelectArgument(const int Stage, const int InValue)"));
		ASSERT_THAT(IsNotNull(CaptureOffRun));
		ASSERT_THAT(IsNotNull(CaptureOnRun));
		ASSERT_THAT(IsNotNull(MixinFunction));
		ASSERT_THAT(IsNotNull(ExternalFunction));
		ASSERT_THAT(IsNotNull(SelectReceiver));
		ASSERT_THAT(IsNotNull(SelectArgument));
		if (CaptureOffRun == nullptr || CaptureOnRun == nullptr
			|| MixinFunction == nullptr || ExternalFunction == nullptr
			|| SelectReceiver == nullptr || SelectArgument == nullptr)
		{
			return;
		}

		const FExecutionObservation CaptureOffExecution = ExecuteGlobalInt(
			CaptureOffEngine.Get(),
			CaptureOffRun);
		const FExecutionObservation CaptureOnExecution = ExecuteGlobalInt(
			CaptureOnEngine.Get(),
			CaptureOnRun);
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, CaptureOffExecution.State));
		ASSERT_THAT(AreEqual(asEXECUTION_FINISHED, CaptureOnExecution.State));
		ASSERT_THAT(AreEqual(
			321120252,
			CaptureOffExecution.ReturnValue,
			TEXT("The VM oracle must evaluate C, default B, A, then the mixin receiver exactly once")));
		ASSERT_THAT(AreEqual(
			CaptureOffExecution.ReturnValue,
			CaptureOnExecution.ReturnValue,
			TEXT("HIR capture must not change mixin execution")));

		const asCTypedSemanticFunction* const RunHIR =
			CaptureOnRun->GetTypedSemanticFunction();
		const asCTypedSemanticFunction* const MixinHIR =
			MixinFunction->GetTypedSemanticFunction();
		const asCTypedSemanticFunction* const ExternalHIR =
			ExternalFunction->GetTypedSemanticFunction();
		ASSERT_THAT(IsNotNull(RunHIR));
		ASSERT_THAT(IsNotNull(MixinHIR));
		ASSERT_THAT(IsNotNull(ExternalHIR));
		if (RunHIR == nullptr || MixinHIR == nullptr || ExternalHIR == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			asETypedSemanticReceiverKind::MixinFirstParameter,
			MixinHIR->header.receiver.kind));
		ASSERT_THAT(AreEqual(0, MixinHIR->header.receiver.parameterIndex));
		ASSERT_THAT(IsTrue(
			MixinFunction->traits.GetTrait(asTRAIT_MIXIN)));
		ASSERT_THAT(IsFalse(
			MixinFunction->traits.GetTrait(asTRAIT_EXTERNAL_IMPLICIT_THIS)));
		ASSERT_THAT(AreEqual(
			asETypedSemanticReceiverKind::ExternalImplicitThis,
			ExternalHIR->header.receiver.kind,
			TEXT("A mixin formal-zero receiver must remain distinct from external implicit this")));

		const asSTypedSemanticExpression* const MixinCall =
			FindResolvedCall(*RunHIR, MixinFunction->id);
		ASSERT_THAT(IsNotNull(MixinCall,
			TEXT("The overload-selected method syntax must publish a resolved mixin call")));
		if (MixinCall == nullptr)
		{
			return;
		}

		ASSERT_THAT(AreEqual(asUINT{4}, MixinCall->operands.GetLength()));
		ASSERT_THAT(AreEqual(
			MixinCall->operands.GetLength(),
			MixinCall->formalParameterIndices.GetLength()));
		ASSERT_THAT(AreEqual(
			MixinCall->operands.GetLength(),
			MixinCall->evaluationSequence.GetLength()));

		const int32 ReceiverBinding = FindFormalBinding(*MixinCall, 0);
		const int32 ABinding = FindFormalBinding(*MixinCall, 1);
		const int32 BBinding = FindFormalBinding(*MixinCall, 2);
		const int32 CBinding = FindFormalBinding(*MixinCall, 3);
		ASSERT_THAT(AreNotEqual(INDEX_NONE, ReceiverBinding));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, ABinding));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, BBinding));
		ASSERT_THAT(AreNotEqual(INDEX_NONE, CBinding));
		if (ReceiverBinding == INDEX_NONE || ABinding == INDEX_NONE
			|| BBinding == INDEX_NONE || CBinding == INDEX_NONE)
		{
			return;
		}

		ASSERT_THAT(IsTrue(MixinCall->receiverExpression.IsValid(),
			TEXT("Mixin method syntax must retain a dedicated source receiver")));
		ASSERT_THAT(AreEqual(
			MixinCall->operands[ReceiverBinding].value,
			MixinCall->receiverExpression.value,
			TEXT("The source receiver must map to the target's real formal zero")));
		asUINT ReceiverOperandCount = 0;
		for (asUINT OperandIndex = 0;
			 OperandIndex < MixinCall->operands.GetLength();
			 ++OperandIndex)
		{
			ReceiverOperandCount += MixinCall->operands[OperandIndex]
				== MixinCall->receiverExpression;
		}
		ASSERT_THAT(AreEqual(asUINT{1}, ReceiverOperandCount,
			TEXT("The receiver expression must occur once in the effective formal mapping")));
		const asSTypedSemanticExpression* const ReceiverExpression =
			ResolveExpression(*RunHIR, MixinCall->receiverExpression);
		ASSERT_THAT(IsNotNull(ReceiverExpression));
		if (ReceiverExpression == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			SelectReceiver->id,
			ReceiverExpression->resolvedFunctionId,
			TEXT("The dedicated receiver must be the effectful SelectReceiver call")));

		ASSERT_THAT(IsTrue(CallHasIntArguments(
			*RunHIR,
			MixinCall->operands[ABinding],
			SelectArgument->id,
			"2",
			"4"),
			TEXT("Named A must retain its selected expression at formal one")));
		ASSERT_THAT(IsTrue(ExpressionIsIntLiteral(
			*RunHIR,
			MixinCall->operands[BBinding],
			"5"),
			TEXT("The omitted B parameter must retain the compiled default expression")));
		ASSERT_THAT(IsTrue(CallHasIntArguments(
			*RunHIR,
			MixinCall->operands[CBinding],
			SelectArgument->id,
			"3",
			"7"),
			TEXT("Named C must retain its selected expression at formal three")));

		ASSERT_THAT(AreEqual(asUINT{3},
			MixinCall->evaluationSequence[ReceiverBinding]));
		ASSERT_THAT(AreEqual(asUINT{2},
			MixinCall->evaluationSequence[ABinding]));
		ASSERT_THAT(AreEqual(asUINT{1},
			MixinCall->evaluationSequence[BBinding]));
		ASSERT_THAT(AreEqual(asUINT{0},
			MixinCall->evaluationSequence[CBinding],
			TEXT("The HIR order must match the maintained reverse-formal VM oracle")));
		ASSERT_THAT(IsTrue(VerifyTypedSemanticFunction(*RunHIR).IsValid()));

		asCTypedSemanticFunction MismatchedReceiver = *RunHIR;
		MismatchedReceiver.expressions[MixinCall->id.value]
			.receiverExpression = MixinCall->operands[ABinding];
		const asSTypedSemanticVerificationResult InvalidResult =
			VerifyTypedSemanticFunction(MismatchedReceiver);
		ASSERT_THAT(AreEqual(
			asETypedSemanticVerificationError::InvalidArgumentProvenance,
			InvalidResult.error,
			TEXT("A receiver that disagrees with its captured formal-zero provenance must fail closed")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
